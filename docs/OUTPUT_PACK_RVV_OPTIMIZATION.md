# RISC-V 输出比特打包 RVV 优化

这次在已有 BG1/Zc384 LDPC 校验内核优化之上，优化 `write_task_output()` 的 RISC-V 输出打包。编码方程、码块长度、输出位序及按位或合并语义保持一致。

## 接入与适用范围

源码目录为 `vendor/openairinterface5g/openair1/PHY/CODING/nrLDPC_coding/nrLDPC_coding_segment/`：

- `nrLDPC_coding_segment_encoder.c`：RVV 目标选择新实现，保留原 RISC-V 标量分支及其他架构分支。
- `write_task_output_rvv.h`：每批最多读取64个中间字节，提取当前码块对应的比特，利用 RVV 掩码打包成最多8个输出字节。

先处理未对齐的起始比特，中间仅处理完整输出字节，尾部继续标量处理。通过 `vlm`、`vmor`、`vsm` 保留已有输出中的有效比特。实际 VL 从硬件取得，并调整为8的整数倍。

本优化与上一轮仅针对 BG1/Zc384 的校验内核不同：输出打包路径不按 BG/Zc 限定，但本轮性能数据仅来自下述配置，不能推广为所有码块规模都同样加速。

RVV 编译时自动启用，可通过 `-DXSAI_LDPC_DISABLE_RVV_OUTPUT_PACK=1` 单独关闭。上一轮校验优化仍可通过 `-DXSAI_LDPC_DISABLE_RVV_BG1_Z384=1` 单独关闭。

## 构建与测试

新克隆者按项目指南安装依赖，然后准备包含最新 vendor 源码的新工作树：

```bash
bash scripts/prepare-oai-worktree.sh "$HOME/xsai-oai-rvv"
```

按本机原有 OAI 流程配置该新源码目录，构建其编码库：

```bash
# OAI_BUILD 应指向上述新源码对应的构建目录
cmake --build "$OAI_BUILD" --target ldpc -j2
```

这个步骤不会替换其他目录已有的生产库。确认运行程序实际加载新构建的 `libldpc.so`，否则源码更新不代表运行版本更新。

在有 RVV 的 RISC-V Linux 上运行独立正确性对照，无需先构建整个 OAI：

```bash
python3 scripts/check-output-pack-rvv.py --cc gcc-16 --cpu 0

# 或测试由本仓库准备出的工作树
python3 scripts/check-output-pack-rvv.py \
  --oai "$HOME/xsai-oai-rvv" --cc gcc-16 --cpu 0
```

默认 CPU 为当前进程允许集合中的最小编号；也可显式指定。测试以保留的标量分支作为参考，检查96,768组组合：1至8码块、0至63bit输出偏移、零长度及跨8/16/32/64等边界的长度、E/E2分组、零/全1/随机初始输出。输入紧邻不可访问保护页，整个输出及保护区逐字节比较。该测试验证与原实现一致，不是独立标准向量认证。

## K3 X100 单线程性能

2026-10-30，X100 CPU0、2.2GHz；50RB分配、106RB载波、MCS14、13符号、两个码块。3种子，各10次预热、200正式TB；每实现共600正式TB。

**两组均已包含上一轮 LDPC 校验优化**，区别仅为输出打包。运行顺序交替。每进程生成一份随机载荷并重复编码。

| 范围 | 原标量打包 | RVV打包 | 下降 |
|---|---:|---:|---:|
| 输出打包（分步计时），μs/TB | 73.481 | 4.450 | 93.94% |
| 整个编码（无分步计时），μs/TB | 235.212 | 164.532 | 30.05% |
| 实测CPU cycles/TB | 511,098 | 361,532 | 29.26% |

局部时间来自分阶段插桩副本，总耗时及周期来自无分步插桩的完整编码 benchmark，不能混用相加。周期为 `perf_event_open` 用户态CPU cycles，排除内核/hypervisor，无频率换算或缩放；未扣除空窗口开销。

历史实测汇总见 [output-pack-rvv-20261030.json](benchmarks/output-pack-rvv-20261030.json)。本次发布源码的再次编译与集成检查见 [发布验证记录](benchmarks/output-pack-rvv-integration-20261030.json)。

候选实现的600正式分阶段TB均在计时外与原生产编码器逐字节一致；完整codec benchmark的输入/输出哈希按种子一致，后续PDSCH检查通过。发布验证不将历史计时冒充重新测得的性能。

本轮未进行新打包实现的 RFsim/真实空口回归，未自动安装至 K3 原生产目录；其他架构没有在本次发布中重新跑原生性能测试。

## 后续旧条件 RFsim 验证

后续在50RB、下行MCS9/BG1/Zc224条件下，完成原版、已有输出打包优化、新增Zc224校验优化的三组RFsim对照；正式窗口错误计数无增长。见[Zc224优化与RFsim记录](LDPC_BG1_Z224_RVV_OPTIMIZATION.md)。本页“本轮未进行RFsim”描述的是输出打包初次发布时点；原始MCS14单线程性能记录仍按本页条件解释。
