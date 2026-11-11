// paperplane.c · 纸飞机 PaperPlane 0.5.1
// 单二进制双模式：
//   ① CUPS filter 模式：cupsd 调用 `paperplane job user title copies options [file]`
//      → 把 CUPS raster 转成打印机指令（TSPL / ZPL / ESC-POS / 透传）
//   ② CLI 模式：`paperplane convert|preview|version ...`
//      → 不出纸预演：PDF 转指令流存文件、或渲染成 PNG 让人眼看效果
//
// 架构说明（v2.0 起的验证结论，勿改）：
//   macOS 26 已不再执行 PPD 里 application/pdf 的第三方 filter，打印管线固定
//   走系统自带 cgpdftoraster。所以本二进制必须接在 application/vnd.cups-raster
//   之后。PPD 里写：*cupsFilter: "application/vnd.cups-raster 0 /Library/.../paperplane"
//
// 硬性约束：
//   · filter 模式下不联网、不起子进程、不写除日志外的任何文件；
//   · filter 必须由 root 安装到 /Library/Printers/PaperPlane/（macOS 26 的
//     cupsd 会拒绝执行非 root 拥有的 filter，报 "Invalid argument"）；
//   · 绝不擅自提交打印作业，验证一律走 convert / 虚拟打印机。
//
// 编译：clang -O2 -Wall -Wextra -arch arm64 -arch x86_64 -o paperplane \
//              paperplane.c -lcups

#define _DARWIN_C_SOURCE 1
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <mach-o/dyld.h>
#include <cups/raster.h>

#define PB_VERSION "0.5.1"
#define LOG_PATH "/tmp/paperplane-filter.log"
#define SETTINGS_PATH "/Library/Application Support/PaperPlane/device-settings.json"
#define SETTINGS_FALLBACK "/Users/Shared/PaperPlane/device-settings.json"

typedef enum { PDL_TSPL = 0, PDL_ZPL = 1, PDL_ESCPOS = 2, PDL_PASSTHROUGH = 3 } Pdl;
typedef enum { MEDIA_GAP = 0, MEDIA_BLACKMARK = 1, MEDIA_CONTINUOUS = 2 } MediaType;

typedef struct {
    double width_mm, height_mm, gap_mm, offset_y_mm, max_width_mm;
    int    density, speed, direction, rotate180, invert, tear_off, dpi;
    int    offset_y_dot;
    int    pdl, media_type, cut;
    char   source[32];
} Config;

/* ── 小工具 ─────────────────────────────────────────────────────────── */

static void emit(const char *s) { fputs(s, stdout); fputs("\r\n", stdout); }

static void logline(const char *fmt, ...) {
    FILE *fp = fopen(LOG_PATH, "a");
    if (!fp) return;
    time_t t = time(NULL);
    char ts[32];
    struct tm tmv;
    localtime_r(&t, &tmv);
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", &tmv);
    fprintf(fp, "%s ", ts);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(fp, fmt, ap);
    va_end(ap);
    fputc('\n', fp);
    fclose(fp);
    chmod(LOG_PATH, 0666);
}

static int opt_str(const char *opts, const char *key, char *out, size_t n) {
    if (!opts || !key) return 0;
    size_t kl = strlen(key);
    const char *p = opts;
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        const char *s = p;
        while (*p && *p != ' ' && *p != '=') p++;
        size_t len = (size_t)(p - s);
        if (*p == '=') {
            p++;
            const char *v = p;
            size_t vl;
            if (*p == '"') {
                p++; v = p;
                while (*p && *p != '"') p++;
                vl = (size_t)(p - v);
                if (*p == '"') p++;
            } else {
                while (*p && *p != ' ') p++;
                vl = (size_t)(p - v);
            }
            if (len == kl && strncmp(s, key, kl) == 0) {
                if (vl >= n) vl = n - 1;
                memcpy(out, v, vl); out[vl] = 0; return 1;
            }
        } else {
            while (*p && *p != ' ') p++;
        }
        if (p == s) p++;
    }
    return 0;
}

static double opt_num(const char *opts, const char *key, double def) {
    char b[64];
    if (opt_str(opts, key, b, sizeof(b))) {
        char *end = NULL;
        double v = strtod(b, &end);
        if (end != b) return v;
    }
    return def;
}

static const char *seek_json_value(const char *js, const char *key) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(js, pat);
    if (!p) return NULL;
    p += strlen(pat);
    while (*p && (*p == ':' || isspace((unsigned char)*p))) p++;
    return (*p) ? p : NULL;
}

static int json_num(const char *js, const char *key, double *out) {
    const char *p = seek_json_value(js, key);
    if (!p) return 0;
    if (*p == 't') { *out = 1; return 1; }
    if (*p == 'f') { *out = 0; return 1; }
    char *end = NULL;
    double v = strtod(p, &end);
    if (end == p) return 0;
    *out = v;
    return 1;
}

static int json_str(const char *js, const char *key, char *out, size_t n) {
    const char *p = seek_json_value(js, key);
    if (!p || *p != '"') return 0;
    p++;
    const char *s = p;
    while (*p && *p != '"') p++;
    size_t l = (size_t)(p - s);
    if (l >= n) l = n - 1;
    memcpy(out, s, l);
    out[l] = 0;
    return 1;
}

static double jsnum(const char *js, const char *key, double def) {
    double v;
    return json_num(js, key, &v) ? v : def;
}

static const char *g_settings_used = "(none)";

static void load_settings(char *buf, size_t n) {
    buf[0] = 0;
    FILE *fp = fopen(SETTINGS_PATH, "r");
    if (fp) g_settings_used = SETTINGS_PATH;
    else {
        fp = fopen(SETTINGS_FALLBACK, "r");
        if (fp) g_settings_used = SETTINGS_FALLBACK;
    }
    if (!fp) return;
    size_t got = fread(buf, 1, n - 1, fp);
    buf[got] = 0;
    fclose(fp);
}

static double env_num(const char *name, double def) {
    const char *s = getenv(name);
    if (!s || !*s) return def;
    char *end = NULL;
    double v = strtod(s, &end);
    return end == s ? def : v;
}

static int parse_pdl(const char *s) {
    if (!s || !*s) return PDL_TSPL;
    if (!strcasecmp(s, "tspl") || !strcasecmp(s, "tspl-label") || !strcasecmp(s, "tspl_label")) return PDL_TSPL;
    if (!strcasecmp(s, "zpl") || !strcasecmp(s, "zpl-label") || !strcasecmp(s, "zpl_label")) return PDL_ZPL;
    if (!strcasecmp(s, "escpos") || !strcasecmp(s, "esc/pos") || !strcasecmp(s, "esc_pos")
        || !strcasecmp(s, "escpos-receipt") || !strcasecmp(s, "escpos_label")) return PDL_ESCPOS;
    if (!strcasecmp(s, "native") || !strcasecmp(s, "passthrough") || !strcasecmp(s, "sheet_native")) return PDL_PASSTHROUGH;
    return PDL_TSPL;
}

static int parse_media(const char *s) {
    if (!s || !*s) return MEDIA_GAP;
    if (!strcasecmp(s, "blackmark") || !strcasecmp(s, "black-mark") || !strcasecmp(s, "mark")) return MEDIA_BLACKMARK;
    if (!strcasecmp(s, "continuous") || !strcasecmp(s, "continuous-roll")) return MEDIA_CONTINUOUS;
    return MEDIA_GAP;
}

static const char *pdl_name(int p) {
    switch (p) {
        case PDL_ZPL: return "zpl";
        case PDL_ESCPOS: return "escpos";
        case PDL_PASSTHROUGH: return "passthrough";
        default: return "tspl";
    }
}

static const char *media_name(int m) {
    switch (m) {
        case MEDIA_BLACKMARK: return "blackmark";
        case MEDIA_CONTINUOUS: return "continuous";
        default: return "gap";
    }
}

static Config load_config(const char *opts) {
    Config c;
    memset(&c, 0, sizeof(c));
    char js[8192];
    load_settings(js, sizeof(js));
    int has_json = (js[0] != 0);

    /* 默认值与 0.4.x 完全一致，保证得力链路零回归 */
    c.width_mm     = 100; c.height_mm = 100; c.gap_mm = 2;
    c.density      = 8;   c.speed = 4;  c.direction = 1;
    c.rotate180    = 1;   c.invert = 1; c.tear_off = 1;
    c.offset_y_mm  = 0;   c.offset_y_dot = 0; c.dpi = 300;
    c.pdl          = PDL_TSPL;
    c.max_width_mm = 100.0;
    c.media_type   = MEDIA_GAP;
    c.cut          = 0;
    strcpy(c.source, "builtin");

    if (has_json) {
        c.width_mm     = jsnum(js, "width_mm",    c.width_mm);
        c.height_mm    = jsnum(js, "height_mm",   c.height_mm);
        c.gap_mm       = jsnum(js, "gap_mm",      c.gap_mm);
        c.density      = (int)jsnum(js, "density", c.density);
        c.speed        = (int)jsnum(js, "speed",   c.speed);
        c.direction    = (int)jsnum(js, "direction", c.direction);
        c.rotate180    = (int)jsnum(js, "rotate180", c.rotate180);
        c.invert       = (int)jsnum(js, "invert",  c.invert);
        c.tear_off     = (int)jsnum(js, "tear_off", c.tear_off);
        c.offset_y_dot = (int)jsnum(js, "offset_y_dot", c.offset_y_dot);
        c.dpi          = (int)jsnum(js, "dpi",     c.dpi);
        c.max_width_mm = jsnum(js, "max_width_mm", c.max_width_mm);
        c.cut          = (int)jsnum(js, "cut",     c.cut);
        char s[64];
        if (json_str(js, "pdl", s, sizeof(s)))        c.pdl = parse_pdl(s);
        if (json_str(js, "media_type", s, sizeof(s))) c.media_type = parse_media(s);
        strcpy(c.source, "settings.json");
    }

    /* 队列级配置：/Users/Shared/PaperPlane/queues/<队列名>.json
       这是「一队列一事实」的关键——全局 settings.json 不得跨队列串味，
       否则 ZPL 队列会被 TSPL 的全局配置覆盖（0.5.1 实测抓到过）。 */
    char qbuf[4096];
    qbuf[0] = 0;
    const char *qname = getenv("PRINTER");
    if (qname && *qname) {
        char qp[1024];
        snprintf(qp, sizeof(qp), "/Users/Shared/PaperPlane/queues/%s.json", qname);
        FILE *qf = fopen(qp, "r");
        if (!qf) {
            snprintf(qp, sizeof(qp),
                     "/Library/Application Support/PaperPlane/queues/%s.json", qname);
            qf = fopen(qp, "r");
        }
        if (qf) {
            size_t got = fread(qbuf, 1, sizeof(qbuf) - 1, qf);
            qbuf[got] = 0;
            fclose(qf);
        }
    }
    if (qbuf[0]) {
        c.width_mm     = jsnum(qbuf, "width_mm",    c.width_mm);
        c.height_mm    = jsnum(qbuf, "height_mm",   c.height_mm);
        c.gap_mm       = jsnum(qbuf, "gap_mm",      c.gap_mm);
        c.density      = (int)jsnum(qbuf, "density", c.density);
        c.speed        = (int)jsnum(qbuf, "speed",   c.speed);
        c.direction    = (int)jsnum(qbuf, "direction", c.direction);
        c.rotate180    = (int)jsnum(qbuf, "rotate180", c.rotate180);
        c.invert       = (int)jsnum(qbuf, "invert",  c.invert);
        c.tear_off     = (int)jsnum(qbuf, "tear_off", c.tear_off);
        c.offset_y_dot = (int)jsnum(qbuf, "offset_y_dot", c.offset_y_dot);
        c.dpi          = (int)jsnum(qbuf, "dpi",     c.dpi);
        c.max_width_mm = jsnum(qbuf, "max_width_mm", c.max_width_mm);
        c.cut          = (int)jsnum(qbuf, "cut",     c.cut);
        char s2[64];
        if (json_str(qbuf, "pdl", s2, sizeof(s2)))        c.pdl = parse_pdl(s2);
        if (json_str(qbuf, "media_type", s2, sizeof(s2))) c.media_type = parse_media(s2);
        strcpy(c.source, "queue.json");
    }

    /* 兜底：直接读本队列 PPD 里的 *DefaultPBPdl，避免队列 json 缺失时串味 */
    if (!qbuf[0]) {
        const char *ppdp = getenv("PPD");
        if (ppdp && *ppdp) {
            FILE *pf = fopen(ppdp, "r");
            if (pf) {
                char line[512];
                while (fgets(line, sizeof(line), pf)) {
                    if (!strncmp(line, "*DefaultPBPdl:", 14)) {
                        char v[64];
                        if (sscanf(line + 14, " %63s", v) == 1) c.pdl = parse_pdl(v);
                        break;
                    }
                }
                fclose(pf);
                strcpy(c.source, "ppd-default");
            }
        }
    }

    /* CUPS options（PPD 默认项优先于 settings.json，便于按队列区分） */
    int had_opt = 0;
    char b[64];
    double v;
    if (opt_str(opts, "DeliSizeW", b, sizeof(b))) { c.width_mm = atof(b); had_opt = 1; }
    if (opt_str(opts, "DeliSizeH", b, sizeof(b))) { c.height_mm = atof(b); had_opt = 1; }
    v = opt_num(opts, "DeliGap", -999);       if (v != -999) { c.gap_mm = v; had_opt = 1; }
    v = opt_num(opts, "DeliDensity", -999);   if (v != -999) { c.density = (int)v; had_opt = 1; }
    v = opt_num(opts, "DeliSpeed", -999);     if (v != -999) { c.speed = (int)v; had_opt = 1; }
    v = opt_num(opts, "DeliDirection", -999); if (v != -999) { c.direction = (int)v; had_opt = 1; }
    v = opt_num(opts, "DeliRotate180", -999); if (v != -999) { c.rotate180 = (int)v; had_opt = 1; }
    v = opt_num(opts, "DeliInvert", -999);    if (v != -999) { c.invert = (int)v; had_opt = 1; }
    v = opt_num(opts, "DeliTearOff", -999);   if (v != -999) { c.tear_off = (int)v; had_opt = 1; }
    v = opt_num(opts, "DeliOffsetY", -999);   if (v != -999) { c.offset_y_dot = (int)v; had_opt = 1; }
    v = opt_num(opts, "DeliDpi", -999);       if (v != -999) { c.dpi = (int)v; had_opt = 1; }
    if (opt_str(opts, "PBPdl", b, sizeof(b)))        { c.pdl = parse_pdl(b); had_opt = 1; }
    if (opt_str(opts, "PBMediaType", b, sizeof(b)))  { c.media_type = parse_media(b); had_opt = 1; }
    if (had_opt) strcpy(c.source, has_json ? "cups-options+settings.json" : "cups-options");

    /* 离线实测覆盖（PB_* 环境变量），最高优先级，仅测试使用 */
    const char *ov = getenv("PP_OVERRIDE");
    if (ov && *ov) {
        c.width_mm     = env_num("PP_WIDTH_MM",     c.width_mm);
        c.height_mm    = env_num("PP_HEIGHT_MM",    c.height_mm);
        c.gap_mm       = env_num("PP_GAP_MM",       c.gap_mm);
        c.density      = (int)env_num("PP_DENSITY", c.density);
        c.speed        = (int)env_num("PP_SPEED",   c.speed);
        c.direction    = (int)env_num("PP_DIRECTION", c.direction);
        c.rotate180    = (int)env_num("PP_ROTATE180", c.rotate180);
        c.invert       = (int)env_num("PP_INVERT",  c.invert);
        c.tear_off     = (int)env_num("PP_TEAR_OFF", c.tear_off);
        c.offset_y_dot = (int)env_num("PP_OFFSET_Y", c.offset_y_dot);
        c.dpi          = (int)env_num("PP_DPI",     c.dpi);
        c.max_width_mm = env_num("PP_MAX_WIDTH_MM", c.max_width_mm);
        c.cut          = (int)env_num("PP_CUT",     c.cut);
        const char *p = getenv("PP_PDL");
        if (p && *p) c.pdl = parse_pdl(p);
        const char *m = getenv("PP_MEDIA_TYPE");
        if (m && *m) c.media_type = parse_media(m);
        strcpy(c.source, "PP_OVERRIDE");
    }
    return c;
}

static inline int is_black(const unsigned char *line, unsigned bpl,
                           unsigned x, int bpp, int colorspace) {
    if (bpp == 1) {
        return (line[x / 8] >> (7 - (x % 8))) & 1;
    }
    unsigned idx;
    if (bpp == 8) {
        idx = x;
        if (idx >= bpl) return 0;
        return line[idx] < 128;
    }
    if (bpp >= 24) {
        unsigned cpp = (unsigned)(bpp / 8);
        idx = x * cpp;
        if (idx + 2 >= bpl) return 0;
        unsigned r = line[idx], g = line[idx + 1], bl = line[idx + 2];
        if (colorspace == CUPS_CSPACE_KCMY || colorspace == CUPS_CSPACE_CMYK) {
            unsigned k = (cpp >= 4) ? line[idx + 3] : 0;
            return (k > 128) || ((r + g + bl) / 3 > 200 && k > 64);
        }
        unsigned lum = (r * 299u + g * 587u + bl * 114u) / 1000u;
        return lum < 128;
    }
    return 0;
}

/* ZPL ^GFA 的数据段是 ASCII HEX（每字节 8 点，1=打印） */
static void write_hex_bitmap(const unsigned char *data, size_t n) {
    static const char HEX[] = "0123456789ABCDEF";
    char out[4096];
    size_t o = 0;
    for (size_t i = 0; i < n; i++) {
        out[o++] = HEX[data[i] >> 4];
        out[o++] = HEX[data[i] & 15];
        if (o >= sizeof(out) - 2) {
            fwrite(out, 1, o, stdout);
            o = 0;
        }
    }
    if (o) fwrite(out, 1, o, stdout);
}

/* ── 位图写出（CLI 预览用） ─────────────────────────────────────────── */

static int write_bmp(const char *path, const unsigned char *mono, unsigned W,
                     unsigned H, unsigned rb) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    unsigned rowbytes = ((W * 3 + 3) / 4) * 4;
    unsigned datasize = rowbytes * H;
    unsigned char hdr[54];
    memset(hdr, 0, sizeof(hdr));
    hdr[0] = 'B'; hdr[1] = 'M';
    unsigned filesize = 54 + datasize;
    hdr[2] = (unsigned char)(filesize); hdr[3] = (unsigned char)(filesize >> 8);
    hdr[4] = (unsigned char)(filesize >> 16); hdr[5] = (unsigned char)(filesize >> 24);
    hdr[10] = 54;
    hdr[14] = 40;
    hdr[18] = (unsigned char)(W); hdr[19] = (unsigned char)(W >> 8);
    hdr[20] = (unsigned char)(W >> 16); hdr[21] = (unsigned char)(W >> 24);
    hdr[22] = (unsigned char)(H); hdr[23] = (unsigned char)(H >> 8);
    hdr[24] = (unsigned char)(H >> 16); hdr[25] = (unsigned char)(H >> 24);
    hdr[26] = 1; hdr[28] = 24;
    hdr[34] = (unsigned char)(datasize); hdr[35] = (unsigned char)(datasize >> 8);
    hdr[36] = (unsigned char)(datasize >> 16); hdr[37] = (unsigned char)(datasize >> 24);
    fwrite(hdr, 1, 54, fp);
    unsigned char *row = (unsigned char *)calloc(rowbytes, 1);
    if (!row) { fclose(fp); return -1; }
    for (unsigned y = 0; y < H; y++) {
        unsigned sy = H - 1 - y;          /* BMP 自下而上 */
        memset(row, 0xFF, rowbytes);
        for (unsigned x = 0; x < W; x++) {
            int bit = (mono[(size_t)sy * rb + x / 8] >> (7 - (x % 8))) & 1;
            if (bit) {
                row[x * 3] = 0; row[x * 3 + 1] = 0; row[x * 3 + 2] = 0;
            }
        }
        fwrite(row, 1, rowbytes, fp);
    }
    free(row);
    fclose(fp);
    return 0;
}

/* ── 核心渲染：CUPS raster → 指令流 / 预览位图 ──────────────────────── */

typedef struct {
    const char *preview_path;   /* 非 NULL = 预览模式，不写指令流 */
    int         preview_done;
    unsigned    preview_w, preview_h, preview_rb;
    unsigned char *preview_mono;
} Sink;

static int render(FILE *in, const Config *c, Sink *sink) {
    cups_raster_t *ras = cupsRasterOpen(fileno(in), CUPS_RASTER_READ);
    if (!ras) {
        logline("ERROR cupsRasterOpen failed");
        fprintf(stderr, "paperplane: 读取 CUPS raster 失败（输入不是合法的 raster 流）\n");
        return 1;
    }

    cups_page_header2_t h;
    int page = 0, rc = 0;
    while (cupsRasterReadHeader2(ras, &h)) {
        page++;
        const unsigned W   = h.cupsWidth;
        const unsigned H   = h.cupsHeight;
        const unsigned bpl = h.cupsBytesPerLine;
        const unsigned rb  = (W + 7) / 8;
        const int bpp = h.cupsBitsPerPixel;
        const double dpi = (h.HWResolution[0] > 0) ? (double)h.HWResolution[0] : (double)c->dpi;
        const double wmm = (double)W / dpi * 25.4;
        const double hmm = (double)H / dpi * 25.4;
        const double use_w = (wmm > 1) ? wmm : c->width_mm;
        const double use_h = (hmm > 1) ? hmm : c->height_mm;

        if (c->max_width_mm > 0 && use_w > c->max_width_mm + 0.2) {
            logline("WARN physical_width_exceeded page=%d width=%.2fmm max=%.2fmm action=no-scaling",
                    page, use_w, c->max_width_mm);
            fprintf(stderr, "paperplane: 警告 作业宽 %.2fmm 超出设备幅宽 %.2fmm，"
                            "未做缩放（不静默改尺寸），请改送支持该纸型的打印机\n",
                    use_w, c->max_width_mm);
        }

        if (!sink || !sink->preview_path) {
            char buf[192];
            if (c->pdl == PDL_TSPL) {
                snprintf(buf, sizeof(buf), "SIZE %.2f mm,%.2f mm", use_w, use_h); emit(buf);
                if (c->media_type == MEDIA_BLACKMARK) {
                    snprintf(buf, sizeof(buf), "BLINE %.2f mm,0 mm", c->gap_mm); emit(buf);
                } else if (c->media_type == MEDIA_CONTINUOUS) {
                    emit("GAP 0 mm,0 mm");
                } else {
                    snprintf(buf, sizeof(buf), "GAP %.2f mm,0 mm", c->gap_mm); emit(buf);
                }
                snprintf(buf, sizeof(buf), "DENSITY %d", c->density); emit(buf);
                snprintf(buf, sizeof(buf), "SPEED %d", c->speed); emit(buf);
                snprintf(buf, sizeof(buf), "DIRECTION %d,0", c->direction); emit(buf);
                emit("REFERENCE 0,0");
                if (c->offset_y_dot != 0) {
                    snprintf(buf, sizeof(buf), "OFFSET %d DOT", c->offset_y_dot); emit(buf);
                } else {
                    emit("OFFSET 0 mm");
                }
                if (c->cut) emit("SET CUTTER ON");
                if (c->tear_off) emit("SET TEAR OFF");
                emit("CLS");
                snprintf(buf, sizeof(buf), "BITMAP 0,0,%u,%u,0,", rb, H);
                fputs(buf, stdout);
            } else if (c->pdl == PDL_ZPL) {
                snprintf(buf, sizeof(buf), "^XA\n"); fputs(buf, stdout);
                snprintf(buf, sizeof(buf), "^PW%u\n", W); fputs(buf, stdout);
                snprintf(buf, sizeof(buf), "^LL%u\n", H); fputs(buf, stdout);
                fputs("^LH0,0\n", stdout);
                if (c->media_type == MEDIA_BLACKMARK) fputs("^MN1\n", stdout);
                else if (c->media_type == MEDIA_CONTINUOUS) fputs("^MN2\n", stdout);
                if (c->density > 0) {
                    int md = c->density > 15 ? 15 : c->density;
                    snprintf(buf, sizeof(buf), "^MD%d\n", md); fputs(buf, stdout);
                }
                size_t total = (size_t)rb * (size_t)H;
                snprintf(buf, sizeof(buf), "^FO0,0^GFA,%zu,%zu,%u,", total, total, rb);
                fputs(buf, stdout);
            } else if (c->pdl == PDL_ESCPOS) {
                const unsigned char esc_init[] = { 0x1B, 0x40 };
                fwrite(esc_init, 1, sizeof(esc_init), stdout);
            }
        }

        unsigned char *line = (unsigned char *)calloc(1, bpl + 8);
        unsigned char *img  = (unsigned char *)calloc(1, (size_t)rb * (size_t)H + 1);
        unsigned char *blk  = (unsigned char *)calloc(1, (size_t)rb * (size_t)H + 1);
        if (!line || !img || !blk) {
            logline("ERROR out of memory W=%u H=%u", W, H);
            fprintf(stderr, "paperplane: 内存不足（%ux%u）\n", W, H);
            rc = 1;
            free(line); free(img); free(blk);
            break;
        }

        for (unsigned y = 0; y < H; y++) {
            memset(line, 0, bpl + 8);
            if (cupsRasterReadPixels(ras, line, bpl) < bpl) {
                /* 数据不足按空白行处理 */
            }
            unsigned char *dst = img + (size_t)y * rb;
            unsigned char *dbk = blk + (size_t)y * rb;
            for (unsigned x = 0; x < W; x++) {
                int black = is_black(line, bpl, x, bpp, (int)h.cupsColorSpace);
                if (black) dbk[x / 8] |= (unsigned char)(0x80 >> (x % 8));
                /* TSPL 固件 mode=0 是「bit=0 打印黑」，invert=1 输出白=1 */
                int set_one = c->invert ? (!black) : black;
                if (set_one) dst[x / 8] |= (unsigned char)(0x80 >> (x % 8));
            }
        }

        /* 预览：只取第一页，按「打印出来是什么样」渲染（真实黑点 + 旋转） */
        if (sink && sink->preview_path && !sink->preview_done) {
            unsigned char *pv = (unsigned char *)calloc(1, (size_t)rb * (size_t)H + 1);
            if (pv) {
                for (unsigned y = 0; y < H; y++) {
                    const unsigned sy = c->rotate180 ? (H - 1 - y) : y;
                    for (unsigned x = 0; x < W; x++) {
                        const unsigned sx = c->rotate180 ? (W - 1 - x) : x;
                        int one = (blk[(size_t)sy * rb + sx / 8] >> (7 - (sx % 8))) & 1;
                        if (one) pv[(size_t)y * rb + x / 8] |= (unsigned char)(0x80 >> (x % 8));
                    }
                }
                sink->preview_mono = pv;
                sink->preview_w = W; sink->preview_h = H; sink->preview_rb = rb;
                sink->preview_done = 1;
            }
        }

        if (!sink || !sink->preview_path) {
            if (c->pdl == PDL_TSPL) {
                unsigned char *row = (unsigned char *)calloc(1, rb + 1);
                if (!row) { rc = 1; free(line); free(img); free(blk); break; }
                for (unsigned y = 0; y < H; y++) {
                    memset(row, 0, rb);
                    const unsigned sy = c->rotate180 ? (H - 1 - y) : y;
                    for (unsigned x = 0; x < W; x++) {
                        const unsigned sx = c->rotate180 ? (W - 1 - x) : x;
                        const int one = (img[(size_t)sy * rb + sx / 8] >> (7 - (sx % 8))) & 1;
                        if (one) row[x / 8] |= (unsigned char)(0x80 >> (x % 8));
                    }
                    fwrite(row, 1, rb, stdout);
                }
                free(row);
                fputs("\r\n", stdout);
                emit("PRINT 1");
                if (c->cut) emit("CUT");
            } else {
                unsigned char *rimg = (unsigned char *)calloc(1, (size_t)rb * (size_t)H + 1);
                if (!rimg) { logline("ERROR out of memory rotated bitmap"); rc = 1; free(line); free(img); free(blk); break; }
                for (unsigned y = 0; y < H; y++) {
                    const unsigned sy = c->rotate180 ? (H - 1 - y) : y;
                    for (unsigned x = 0; x < W; x++) {
                        const unsigned sx = c->rotate180 ? (W - 1 - x) : x;
                        const int one = (blk[(size_t)sy * rb + sx / 8] >> (7 - (sx % 8))) & 1;
                        if (one) rimg[(size_t)y * rb + x / 8] |= (unsigned char)(0x80 >> (x % 8));
                    }
                }
                size_t total = (size_t)rb * (size_t)H;
                if (c->pdl == PDL_ZPL) {
                    write_hex_bitmap(rimg, total);
                    fputs("\n^PQ1\n^XZ\n", stdout);
                } else {
                    /* ESC/POS GS v 0 分带输出：部分廉价固件单条指令上限 255 行 */
                    const unsigned band = 255;
                    for (unsigned y0 = 0; y0 < H; y0 += band) {
                        unsigned rows = (H - y0 < band) ? (H - y0) : band;
                        unsigned char gs[8];
                        gs[0] = 0x1D; gs[1] = 0x76; gs[2] = 0x30; gs[3] = 0x00;
                        gs[4] = (unsigned char)(rb & 0xFF);
                        gs[5] = (unsigned char)((rb >> 8) & 0xFF);
                        gs[6] = (unsigned char)(rows & 0xFF);
                        gs[7] = (unsigned char)((rows >> 8) & 0xFF);
                        fwrite(gs, 1, sizeof(gs), stdout);
                        fwrite(rimg + (size_t)y0 * rb, 1, (size_t)rb * rows, stdout);
                    }
                    if (c->cut) {
                        const unsigned char cutcmd[] = { 0x1D, 0x56, 0x42, 0x00 };
                        const unsigned char feeds[] = { 0x0A, 0x0A, 0x0A };
                        fwrite(feeds, 1, sizeof(feeds), stdout);
                        fwrite(cutcmd, 1, sizeof(cutcmd), stdout);
                    } else {
                        const unsigned char lf[] = { 0x0A, 0x0A, 0x0A };
                        fwrite(lf, 1, sizeof(lf), stdout);
                    }
                }
                free(rimg);
            }
            fflush(stdout);
        }

        logline("PAGE %d pdl=%s %ux%u dots bpl=%u bpp=%d cs=%d res=%.0f => %.2fx%.2f mm "
                "SIZE=%.2fx%.2f GAP=%.2f rot=%d inv=%d",
                page, pdl_name(c->pdl), W, H, bpl, bpp, (int)h.cupsColorSpace,
                dpi, wmm, hmm, use_w, use_h, c->gap_mm, c->rotate180, c->invert);
        free(line);
        free(img);
        free(blk);
    }

    cupsRasterClose(ras);
    if (page == 0) {
        fprintf(stderr, "paperplane: 未读到任何页面\n");
        rc = 1;
    }
    logline("END pages=%d pdl=%s rc=%d", page, pdl_name(c->pdl), rc);
    return rc;
}

/* ── CLI 模式 ───────────────────────────────────────────────────────── */

static const char *arg_opt(int argc, char **argv, const char *name, const char *def) {
    for (int i = 2; i < argc - 1; i++) {
        if (!strcmp(argv[i], name)) return argv[i + 1];
    }
    return def;
}

/* 找 PPD：--ppd > $PB_PPD > 可执行文件同级/../drivers > 系统安装位置 */
static const char *find_ppd(const char *leaf, char *out, size_t n) {
    const char *env = getenv("PB_PPD");
    if (env && *env) { snprintf(out, n, "%s", env); return out; }

    char exe[4096];
    uint32_t sz = sizeof(exe);
    if (_NSGetExecutablePath(exe, &sz) == 0) {
        char real[4096];
        if (realpath(exe, real)) snprintf(exe, sizeof(exe), "%s", real);
        char *slash = strrchr(exe, '/');
        if (slash) {
            *slash = 0;                       /* .../bin */
            char dir[4096];
            snprintf(dir, sizeof(dir), "%s/../drivers/%s", exe, leaf);
            FILE *f = fopen(dir, "r");
            if (f) { fclose(f); snprintf(out, n, "%s", dir); return out; }
        }
    }
    snprintf(out, n, "/Library/Printers/PPDs/Contents/Resources/PaperPlane/%s", leaf);
    return out;
}

static void usage(void) {
    fprintf(stderr,
        "纸飞机 PaperPlane %s\n"
        "\n"
        "  paperplane version                        显示版本\n"
        "  paperplane preview --in a.pdf [--out a.png]\n"
        "                                          不出纸预演：渲染成 PNG 看效果\n"
        "  paperplane convert --in a.pdf --out a.bin [--pdl tspl|zpl|escpos]\n"
        "                                          PDF 转成打印机指令流存文件\n"
        "        可选：--dpi 203/300/600  --media gap|blackmark|continuous\n"
        "              --gap 2  --density 8  --rotate180 0|1  --invert 0|1  --cut 0|1\n"
        "\n"
        "  CUPS filter 模式由 cupsd 自动调用，无需手动执行。\n", PB_VERSION);
}

static char *slurp_pipe(const char *cmdline, size_t *out_len) {
    FILE *p = popen(cmdline, "r");
    if (!p) return NULL;
    size_t cap = 1 << 20, len = 0;
    unsigned char *buf = (unsigned char *)malloc(cap);
    if (!buf) { pclose(p); return NULL; }
    size_t n;
    while ((n = fread(buf + len, 1, cap - len, p)) > 0) {
        len += n;
        if (len == cap) {
            cap *= 2;
            unsigned char *nb = (unsigned char *)realloc(buf, cap);
            if (!nb) { free(buf); pclose(p); return NULL; }
            buf = nb;
        }
    }
    pclose(p);
    *out_len = len;
    return (char *)buf;
}

static int cli_main(int argc, char **argv) {
    const char *cmd = argv[1];

    if (!strcmp(cmd, "version") || !strcmp(cmd, "--version") || !strcmp(cmd, "-v")) {
        printf("%s\n", PB_VERSION);
        return 0;
    }
    if (!strcmp(cmd, "help") || !strcmp(cmd, "--help") || !strcmp(cmd, "-h")) {
        usage();
        return 0;
    }

    const char *in  = arg_opt(argc, argv, "--in", NULL);
    const char *out = arg_opt(argc, argv, "--out", NULL);
    if (!in) { usage(); return 2; }

    Config c = load_config("");
    c.pdl = parse_pdl(arg_opt(argc, argv, "--pdl", "tspl"));
    const char *media = arg_opt(argc, argv, "--media", NULL);
    if (media) c.media_type = parse_media(media);
    c.dpi       = (int)atof(arg_opt(argc, argv, "--dpi", "300"));
    c.gap_mm    = atof(arg_opt(argc, argv, "--gap", "2"));
    c.density   = (int)atof(arg_opt(argc, argv, "--density", "8"));
    c.invert    = (int)atof(arg_opt(argc, argv, "--invert", "1"));
    c.rotate180 = (int)atof(arg_opt(argc, argv, "--rotate180", "1"));
    c.cut       = (int)atof(arg_opt(argc, argv, "--cut", "0"));
    c.max_width_mm = atof(arg_opt(argc, argv, "--max-width", "120"));
    strcpy(c.source, "cli");

    /* PDF → CUPS raster：走系统 cgpdftoraster，与真实打印管线同一条路 */
    const char *leaf = (c.pdl == PDL_ZPL) ? "zpl_label.ppd"
                     : (c.pdl == PDL_ESCPOS) ? "escpos_receipt.ppd"
                     : "tspl_label.ppd";
    char ppd[4096];
    const char *ppd_arg = arg_opt(argc, argv, "--ppd", NULL);
    if (ppd_arg && *ppd_arg) snprintf(ppd, sizeof(ppd), "%s", ppd_arg);
    else find_ppd(leaf, ppd, sizeof(ppd));
    if (access(ppd, R_OK) != 0) {
        fprintf(stderr, "paperplane: 找不到 PPD：%s\n（可用 --ppd 指定，或先运行安装脚本）\n", ppd);
        return 1;
    }

    char cmdline[8192];
    snprintf(cmdline, sizeof(cmdline),
             "cupsfilter -p '%s' -i application/pdf -m application/vnd.cups-raster "
             "-o Resolution=%ddpi \"%s\" 2>/dev/null",
             ppd, c.dpi, in);

    size_t rlen = 0;
    char *raster = slurp_pipe(cmdline, &rlen);
    if (!raster || rlen < 4) {
        fprintf(stderr, "paperplane: 无法把 %s 转成 raster（cupsfilter 失败）\n", in);
        free(raster);
        return 1;
    }

    FILE *tmp = tmpfile();
    if (!tmp) { free(raster); return 1; }
    fwrite(raster, 1, rlen, tmp);
    free(raster);
    rewind(tmp);

    Sink sink;
    memset(&sink, 0, sizeof(sink));
    int is_preview = !strcmp(cmd, "preview");
    if (is_preview) sink.preview_path = out ? out : "preview.bmp";
    else {
        if (!out) { usage(); fclose(tmp); return 2; }
        FILE *o = freopen(out, "wb", stdout);
        if (!o) { perror("paperplane: 输出文件"); fclose(tmp); return 1; }
    }

    int rc = render(tmp, &c, is_preview ? &sink : NULL);
    fclose(tmp);
    fflush(stdout);

    if (is_preview) {
        if (!sink.preview_mono) { fprintf(stderr, "paperplane: 预览渲染失败\n"); return 1; }
        char bmp[4096];
        snprintf(bmp, sizeof(bmp), "%s.bmp", sink.preview_path);
        if (write_bmp(bmp, sink.preview_mono, sink.preview_w, sink.preview_h,
                      sink.preview_rb) != 0) {
            fprintf(stderr, "paperplane: 无法写出预览图\n");
            free(sink.preview_mono);
            return 1;
        }
        /* sips 是 macOS 自带，转 PNG 便于直接双击查看 */
        char cmd2[8192];
        snprintf(cmd2, sizeof(cmd2), "sips -s format png '%s' --out '%s' >/dev/null 2>&1",
                 bmp, sink.preview_path);
        if (system(cmd2) == 0) unlink(bmp);
        fprintf(stderr, "预演图：%s （%ux%u 点，按实际打印方向渲染；未送出任何纸张）\n",
                sink.preview_path, sink.preview_w, sink.preview_h);
        free(sink.preview_mono);
    } else {
        fprintf(stderr, "已生成 %s（%s，未送出任何纸张）\n", out, pdl_name(c.pdl));
    }
    return rc;
}

/* ── 入口 ───────────────────────────────────────────────────────────── */

int main(int argc, char **argv) {
    /* CUPS filter 调用约定：paperplane job-id user title copies options [file] */
    if (argc >= 2) {
        static const char *cmds[] = { "version", "help", "preview", "convert",
                                      "--version", "-v", "--help", "-h", NULL };
        for (int i = 0; cmds[i]; i++) {
            if (!strcmp(argv[1], cmds[i])) return cli_main(argc, argv);
        }
    }

    const char *opts = (argc >= 6) ? argv[5] : "";
    int fd = 0;
    if (argc >= 7) {
        fd = open(argv[6], O_RDONLY);
        if (fd < 0) { perror("paperplane: open input"); return 1; }
    }

    Config c = load_config(opts);
    logline("START v=%s in=%s opts=%.400s source=%s settings=%s pdl=%s media=%s "
            "cfg=%.0fx%.0fmm gap=%.1f dens=%d dir=%d rot=%d inv=%d tear=%d cut=%d off=%d dpi=%d maxw=%.1f",
            PB_VERSION, (argc >= 7) ? argv[6] : "(stdin)", opts ? opts : "", c.source,
            g_settings_used, pdl_name(c.pdl), media_name(c.media_type),
            c.width_mm, c.height_mm, c.gap_mm, c.density, c.direction,
            c.rotate180, c.invert, c.tear_off, c.cut, c.offset_y_dot, c.dpi, c.max_width_mm);

    int rc;
    if (c.pdl == PDL_PASSTHROUGH) {
        unsigned char buf[65536];
        ssize_t n;
        while ((n = read(fd, buf, sizeof(buf))) > 0) fwrite(buf, 1, (size_t)n, stdout);
        fflush(stdout);
        logline("END pages=0 mode=passthrough (系统原生驱动接管)");
        rc = 0;
    } else {
        FILE *in = fdopen(fd, "rb");
        if (!in) { fprintf(stderr, "paperplane: fdopen 失败\n"); return 1; }
        rc = render(in, &c, NULL);
        fclose(in);
        fd = -1;
    }
    if (fd > 0) close(fd);
    return rc;
}
