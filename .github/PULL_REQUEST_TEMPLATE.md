## 改了什么

<!-- 一句话说清 -->

## 为什么

<!-- 关联 issue 或说明动机 -->

## 怎么验证的

- [ ] `bash scripts/make.sh` 通过
- [ ] `bash install.sh --dry-run` 通过
- [ ] `python3 tests/harness.py` 全绿
- [ ] 若改了驱动输出，附前后字节对比

## 硬规则自查

- [ ] 没有新增「设备不报就默认成某族」的分支
- [ ] 没有静默缩放
- [ ] filter 模式没有联网 / 起子进程
- [ ] 判定逻辑只改了 `lib/resolve.sh`
