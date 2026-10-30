# K3 X100 LDPC RVV 指令优化

本次把2026-10-29已实测的指令优化接入本仓库vendor OAI源码。编码方程、输入/输出位置、数据展开、码率、CRC、分段及速率匹配均不改动。优化针对原128bit布局下的BG1/Zc384校验计算。

## 接入点

- `vendor/openairinterface5g/openair1/PHY/CODING/nrLDPC_encoder/ldpc_encode_parity_check.c`：只在BG1的`case 384`选择新函数。
- 同目录 `ldpc_bg1_z384_rvv.h`：从原`ldpc384_byte_128.c`提取全部46行异或操作数和输出偏移，使用原生RVV加载、异或和存储。每批最多64字节、LMUL2、四累加器，运行时按实际VL推进。
- 仅 `__riscv_vector` 且没有AVX2/AVX512布局宏时启用。其他BG/Zc和非RVV目标保持原实现；并非所有码块规模都已加速。

## 构建

新克隆者先按项目指南安装依赖，并准备一个新的OAI工作目录：

```bash
bash scripts/prepare-oai-worktree.sh "$HOME/xsai-oai-rvv"
```

这一步会复制包含优化的vendor源码，不会编译或修改现有部署。随后按照本机原有OAI流程为**这个新目录**配置CMake并构建。配置完成后可单独构建编码库：

```bash
# OAI_BUILD必须属于上述新源码目录，而不是旧生产工作树
cmake --build "$OAI_BUILD" --target ldpc -j2
```

RVV条件满足时自动选择新内核。对照原版可在对应构建的C编译选项中加入 `-DXSAI_LDPC_DISABLE_RVV_BG1_Z384=1` 并重新编译ldpc目标。不要用新库直接覆盖正在运行的gNB。

## 正确性检查

任意有Python的设备可检查校验表与原OAI全部操作数和目的偏移完全一致：

```bash
python3 -m unittest discover -s tests -p 'test_ldpc_rvv_tables.py' -v
```

在有RVV的K3上执行原内核/新内核的随机矩阵逐字节对照：

```bash
python3 scripts/check-ldpc-rvv.py \
  --oai "$HOME/xsai-oai-rvv" --build "$OAI_BUILD" --cc gcc-16 --cpu 0
```

当前检查脚本读取OAI Makefile构建的`ldpc.dir/flags.make`，不承诺Ninja构建兼容。测试16份独立随机展开矩阵，每份对照17664字节输出，并检查输出尾部保护区。展开矩阵每字节包含多个码块对应的位，不能把这17664字节理解为单个码块17664字节的实际空口数据。

完整TB耗时仍可使用独立[xsai-module-benchmark](https://github.com/feeasolfnj/xsai-module-benchmark)，构建时指定新的OAI源码和构建目录，确保加载的是新构建的libldpc.so。

## 已实测结果

X100单核2.2GHz，50RB、MCS14、13符号、两个码块；三个随机种子，每配置共600正式TB，10次预热排除。原版参考编码和逐字节校验在计时之外。

| 计时范围 | 原版 μs/TB | 指令优化 μs/TB | 缩短 |
|---|---:|---:|---:|
| 整个编码模块 | 336.221 | 234.127 | 30.36% |
| LDPC完整调用 | 234.637 | 133.093 | 43.28% |
| 数据展开与校验计算区域 | 208.685 | 107.261 | 48.60% |

这些是同日独立指令试验的历史结果，见[原始汇总](benchmarks/ldpc-rvv-20261029.json)。本次仓库集成的编译和完整TB检查见[集成验证记录](benchmarks/ldpc-rvv-integration-20261029.json)，不能把历史数字称为每次重建都必然达到的性能。计时范围存在嵌套，不能相加。该轮未采集硬件周期数或退休指令数。

每次试验编码均与原版3750字节有效输出逐字节一致；额外检查6RB/MCS4、106RB/MCS28。代码保留OAI原许可和署名，不更改根目录及组件许可证。尚未完成RFsim/真实空口回归；此次更新不自动安装到K3生产目录。

## 后续输出打包优化

2026-10-30另加入 `write_task_output()` 的RVV批量打包。该修改独立于本页的BG1/Zc384校验内核；两项现均保留在源码中。最新对照与使用方法见[输出打包优化](OUTPUT_PACK_RVV_OPTIMIZATION.md)，本页历史计时不等同于两项优化合并后的结果。

## 后续 BG1/Zc224 扩展

本页记录的是2026-10-29的Zc384优化。随后已新增独立的BG1/Zc224内核及禁用开关`XSAI_LDPC_DISABLE_RVV_BG1_Z224`；旧配置的RFsim回归和新性能对照见[Zc224优化说明](LDPC_BG1_Z224_RVV_OPTIMIZATION.md)。本页历史的“尚未完成RFsim”是原测试时点的边界，不代表后续验证状态。
