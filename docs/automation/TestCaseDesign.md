# TestCaseDesign — 测试用例设计规范

> **文档版本**: v1.0  
> **父文档**: [AutomationFramework.md](AutomationFramework.md)  
> **关联模块**: AutomationEngineService, ParamManagerService, DataBus  

---

## 目标

定义 Motor Studio 自动化测试用例的 JSON 格式规范、步骤组合模式、变量作用域规则、断言表达式语法以及测试数据记录策略。为测试工程师提供标准化的测试用例编写指南，确保测试用例可读、可维护、可复用。

### 核心目标

1. **标准化**：统一的 JSON 格式，所有测试用例遵循相同结构
2. **可组合**：步骤可自由组合，支持循环、分支、子测试嵌套
3. **可复用**：子测试支持参数化，公共测试逻辑抽取为子测试
4. **可读性**：JSON 字段命名清晰，每个步骤有描述性注释
5. **完整性**：测试用例包含前置条件、执行步骤、断言、清理逻辑

---

## 设计原则

| 原则 | 说明 |
|------|------|
| **声明式定义** | 测试用例描述"做什么"而非"怎么做"，执行细节由引擎处理 |
| **显式优于隐式** | 变量、参数、超时全部显式声明，避免隐式默认值 |
| **单一入口单一出口** | 每个测试用一个 `cleanup` 段确保清理逻辑执行 |
| **数据驱动** | 相似的测试场景通过参数化复用，而非复制粘贴 |
| **可追溯** | 每步有 `id` 和 `description`，报告可追溯到具体步骤 |

---

## JSON 测试定义格式

### 完整 Schema

```json
{
  "format_version": "1.0",
  "test": {
    "id": "motor_speed_ramp_001",
    "name": "电机转速斜坡测试",
    "description": "验证电机从 0 到 3000 RPM 的斜坡加速过程中，转速跟随性、电流变化和温度响应",
    "category": "motor",
    "tags": ["speed_control", "ramp", "regression"],
    "author": "Test Team",
    "version": "1.0.0",
    
    "preconditions": {
      "required_params": [
        {"paramId": 65536, "name": "MotorMaxSpeed", "min_value": 3000}
      ],
      "motor_state": "stopped",
      "connection": "connected"
    },
    
    "setup": {
      "description": "测试前准备",
      "steps": [
        {
          "id": "setup_1",
          "type": "SetParameter",
          "description": "设置加速度为 600 RPM/s",
          "paramId": 65540,
          "value": 600
        },
        {
          "id": "setup_2",
          "type": "SetParameter",
          "description": "设置目标转速为 3000 RPM",
          "paramId": 65541,
          "value": 3000
        }
      ]
    },
    
    "steps": [
      {
        "id": "step_1",
        "type": "RecordData",
        "description": "开始记录数据",
        "paramIds": [65536, 65537, 65538, 65539],
        "sample_rate_hz": 100,
        "duration_ms": 0,
        "continuous": true
      },
      {
        "id": "step_2",
        "type": "SendCommand",
        "description": "使能电机",
        "command": "EnableMotor",
        "args": {}
      },
      {
        "id": "step_3",
        "type": "Delay",
        "description": "等待电机启动稳定",
        "duration_ms": 500
      },
      {
        "id": "step_4",
        "type": "RampSpeed",
        "description": "斜坡加速至 3000 RPM",
        "targetSpeed": 3000,
        "rampTime": 5000,
        "steps": 50
      },
      {
        "id": "step_5",
        "type": "WaitFor",
        "description": "等待转速稳定在 3000±50 RPM",
        "condition": "abs(MotorSpeed - 3000) < 50",
        "timeout_ms": 10000,
        "poll_interval_ms": 100
      },
      {
        "id": "step_6",
        "type": "AssertCondition",
        "description": "验证转速在目标范围内",
        "assertions": [
          {
            "type": "value_in_range",
            "paramId": 65536,
            "min": 2950,
            "max": 3050,
            "message": "转速应在 2950-3050 RPM 范围内"
          },
          {
            "type": "value_in_range",
            "paramId": 65537,
            "min": 0,
            "max": 15,
            "message": "稳态电流不应超过 15A"
          }
        ]
      },
      {
        "id": "step_7",
        "type": "RecordData",
        "description": "停止记录数据",
        "paramIds": [65536, 65537, 65538, 65539],
        "continuous": false
      }
    ],
    
    "assertions": [
      {
        "type": "expression",
        "expr": "step_6.result == 'pass'",
        "message": "转速斜坡测试通过"
      },
      {
        "type": "expression",
        "expr": "max(MotorCurrent) < 20",
        "message": "整个测试过程中电流不应超过 20A"
      },
      {
        "type": "no_fault",
        "message": "测试过程中不应出现故障"
      }
    ],
    
    "cleanup": {
      "description": "测试后清理",
      "steps": [
        {
          "id": "cleanup_1",
          "type": "RampSpeed",
          "description": "减速至 0",
          "targetSpeed": 0,
          "rampTime": 3000,
          "steps": 30
        },
        {
          "id": "cleanup_2",
          "type": "SendCommand",
          "description": "禁用电机",
          "command": "DisableMotor",
          "args": {}
        }
      ]
    },
    
    "config": {
      "global_timeout_ms": 120000,
      "continue_on_step_failure": false,
      "record_all_params": false,
      "report_format": "html"
    }
  }
}
```

### Schema 字段说明

| 路径 | 类型 | 必需 | 说明 |
|------|------|------|------|
| `format_version` | string | 是 | 格式版本，当前为 "1.0" |
| `test.id` | string | 是 | 测试用例唯一标识 |
| `test.name` | string | 是 | 测试用例名称 |
| `test.description` | string | 是 | 测试用例描述 |
| `test.category` | string | 否 | 分类：motor, driver, communication, system |
| `test.tags` | array | 否 | 标签，用于筛选和分组 |
| `test.preconditions` | object | 是 | 前置条件 |
| `test.preconditions.required_params` | array | 否 | 必需的参数及其范围 |
| `test.preconditions.motor_state` | enum | 否 | 要求电机状态：stopped, running, any |
| `test.setup` | object | 否 | 测试前准备步骤 |
| `test.steps` | array | 是 | 测试步骤列表 |
| `test.assertions` | array | 否 | 全局断言 |
| `test.cleanup` | object | 是 | 清理步骤（始终执行） |
| `test.config.global_timeout_ms` | uint32 | 否 | 全局超时（默认 300000ms） |
| `test.config.continue_on_step_failure` | bool | 否 | 步骤失败是否继续（默认 false） |
| `test.config.record_all_params` | bool | 否 | 是否记录所有参数（默认 false） |

---

## 示例测试用例

### 示例 1: 电机转速斜坡测试

```json
{
  "format_version": "1.0",
  "test": {
    "id": "motor_speed_ramp_001",
    "name": "电机转速斜坡测试 — 满载加速",
    "description": "满载条件下，电机从 0 加速到额定转速 3000 RPM，验证转速跟随误差 < 5%，电流 < 额定值 1.5 倍",
    "category": "motor",
    "tags": ["speed_control", "ramp", "regression", "full_load"],
    
    "preconditions": {
      "required_params": [
        {"paramId": 65536, "name": "MotorMaxSpeed", "min_value": 3000},
        {"paramId": 65540, "name": "Acceleration", "min_value": 100}
      ],
      "motor_state": "stopped",
      "connection": "connected"
    },
    
    "setup": {
      "steps": [
        {"id": "s1", "type": "SetParameter", "paramId": 65540, "value": 600, "description": "加速度 600 RPM/s"},
        {"id": "s2", "type": "SetParameter", "paramId": 65541, "value": 3000, "description": "目标转速 3000 RPM"}
      ]
    },
    
    "steps": [
      {"id": "1", "type": "RecordData", "paramIds": [65536, 65537, 65542], "sample_rate_hz": 100, "continuous": true, "description": "开始记录转速、电流、转矩"},
      {"id": "2", "type": "SendCommand", "command": "EnableMotor", "description": "使能电机"},
      {"id": "3", "type": "Delay", "duration_ms": 500, "description": "等待启动"},
      {"id": "4", "type": "RampSpeed", "targetSpeed": 3000, "rampTime": 5000, "steps": 50, "description": "斜坡加速"},
      {"id": "5", "type": "WaitFor", "condition": "abs(MotorSpeed - 3000) < 50", "timeout_ms": 10000, "description": "等待转速稳定"},
      {"id": "6", "type": "AssertCondition", "assertions": [
        {"type": "value_in_range", "paramId": 65536, "min": 2850, "max": 3150, "message": "转速误差 < 5%"},
        {"type": "value_in_range", "paramId": 65537, "min": 0, "max": 22.5, "message": "电流 < 15A × 1.5"}
      ], "description": "验证稳态值"},
      {"id": "7", "type": "RecordData", "continuous": false, "description": "停止记录"}
    ],
    
    "cleanup": {
      "steps": [
        {"id": "c1", "type": "RampSpeed", "targetSpeed": 0, "rampTime": 3000, "steps": 30, "description": "减速停止"},
        {"id": "c2", "type": "SendCommand", "command": "DisableMotor", "description": "禁用电机"}
      ]
    }
  }
}
```

### 示例 2: 电流限制保护测试

```json
{
  "format_version": "1.0",
  "test": {
    "id": "protection_current_limit_001",
    "name": "电流限制保护测试",
    "description": "设置电流限制为 5A，让电机堵转，验证电流保护在 5A±10% 触发",
    "category": "motor",
    "tags": ["protection", "current_limit", "safety"],
    
    "preconditions": {
      "required_params": [
        {"paramId": 65542, "name": "CurrentLimit", "min_value": 1, "max_value": 20}
      ],
      "motor_state": "stopped"
    },
    
    "setup": {
      "steps": [
        {"id": "s1", "type": "SetParameter", "paramId": 65542, "value": 5, "description": "电流限制 5A"},
        {"id": "s2", "type": "SetParameter", "paramId": 65540, "value": 1000, "description": "加速度 1000 RPM/s"},
        {"id": "s3", "type": "SetParameter", "paramId": 65541, "value": 1000, "description": "目标转速 1000 RPM"}
      ]
    },
    
    "steps": [
      {"id": "1", "type": "RecordData", "paramIds": [65536, 65537, 65542, 65543], "sample_rate_hz": 500, "continuous": true, "description": "高频记录电流"},
      {"id": "2", "type": "SendCommand", "command": "EnableMotor", "description": "使能电机（堵转状态）"},
      {"id": "3", "type": "WaitFor", "condition": "FaultStatus.OverCurrent == 1", "timeout_ms": 5000, "poll_interval_ms": 10, "description": "等待过流保护触发"},
      {"id": "4", "type": "AssertCondition", "assertions": [
        {"type": "value_in_range", "paramId": 65537, "min": 4.5, "max": 5.5, "message": "触发时电流 5A±10%"}
      ], "description": "验证触发阈值"},
      {"id": "5", "type": "AssertCondition", "assertions": [
        {"type": "expression", "expr": "step_3.elapsed_ms < 5000", "message": "保护应在 5 秒内触发"}
      ]},
      {"id": "6", "type": "RecordData", "continuous": false, "description": "停止记录"}
    ],
    
    "cleanup": {
      "steps": [
        {"id": "c1", "type": "SendCommand", "command": "ClearFaults", "description": "清除故障"},
        {"id": "c2", "type": "SendCommand", "command": "DisableMotor", "description": "禁用电机"}
      ]
    }
  }
}
```

### 示例 3: 温度保护测试

```json
{
  "format_version": "1.0",
  "test": {
    "id": "protection_overtemp_001",
    "name": "过温保护测试",
    "description": "设置过温保护阈值 80°C，持续加载使电机升温，验证 80°C 时触发保护停机",
    "category": "motor",
    "tags": ["protection", "temperature", "safety", "long_running"],
    
    "preconditions": {
      "required_params": [
        {"paramId": 65544, "name": "OverTempThreshold", "value": 80}
      ],
      "motor_state": "stopped"
    },
    
    "config": {
      "global_timeout_ms": 600000
    },
    
    "setup": {
      "steps": [
        {"id": "s1", "type": "SetParameter", "paramId": 65544, "value": 80, "description": "过温阈值 80°C"},
        {"id": "s2", "type": "SetParameter", "paramId": 65541, "value": 2000, "description": "目标转速 2000 RPM"},
        {"id": "s3", "type": "SetParameter", "paramId": 65540, "value": 500, "description": "加速度 500 RPM/s"}
      ]
    },
    
    "steps": [
      {"id": "1", "type": "RecordData", "paramIds": [65536, 65537, 65538, 65543], "sample_rate_hz": 10, "continuous": true, "description": "低速记录温度"},
      {"id": "2", "type": "SendCommand", "command": "EnableMotor", "description": "使能电机"},
      {"id": "3", "type": "RampSpeed", "targetSpeed": 2000, "rampTime": 4000, "steps": 40, "description": "加速至 2000 RPM"},
      {"id": "4", "type": "WaitFor", "condition": "MotorTemperature >= 80", "timeout_ms": 540000, "poll_interval_ms": 1000, "description": "等待温度升至 80°C"},
      {"id": "5", "type": "AssertCondition", "assertions": [
        {"type": "expression", "expr": "FaultStatus.OverTemperature == 1", "message": "过温故障位应置位"},
        {"type": "value_in_range", "paramId": 65538, "min": 78, "max": 85, "message": "触发时温度 80°C±5°C"}
      ], "description": "验证过温保护"},
      {"id": "6", "type": "RecordData", "continuous": false, "description": "停止记录"}
    ],
    
    "cleanup": {
      "steps": [
        {"id": "c1", "type": "SendCommand", "command": "ClearFaults", "description": "清除故障"},
        {"id": "c2", "type": "SendCommand", "command": "DisableMotor", "description": "禁用电机"}
      ]
    }
  }
}
```

### 示例 4: 通信压力测试

```json
{
  "format_version": "1.0",
  "test": {
    "id": "comm_stress_001",
    "name": "通信压力测试 — 1000 次参数读写",
    "description": "连续执行 1000 次参数读写操作，验证通信稳定性，无丢包、无超时",
    "category": "communication",
    "tags": ["stress", "communication", "regression"],
    
    "preconditions": {
      "connection": "connected"
    },
    
    "config": {
      "global_timeout_ms": 300000,
      "continue_on_step_failure": false
    },
    
    "steps": [
      {"id": "1", "type": "RecordData", "paramIds": [65536, 65537], "sample_rate_hz": 0, "continuous": true, "description": "开始记录"},
      {
        "id": "2",
        "type": "Loop",
        "description": "循环 1000 次读写操作",
        "count": 1000,
        "steps": [
          {
            "id": "2_1",
            "type": "SetParameter",
            "paramId": 65541,
            "value": "${loop.index * 3}",
            "description": "递增目标转速",
            "continueOnFailure": false
          },
          {
            "id": "2_2",
            "type": "Delay",
            "duration_ms": 10,
            "description": "间隔 10ms"
          },
          {
            "id": "2_3",
            "type": "AssertCondition",
            "assertions": [
              {"type": "value_in_range", "paramId": 65536, "min": -10, "max": 10000, "message": "转速读取有效"}
            ],
            "description": "验证读取有效"
          }
        ]
      },
      {"id": "3", "type": "RecordData", "continuous": false, "description": "停止记录"}
    ],
    
    "assertions": [
      {"type": "expression", "expr": "count(step_results, 'failed') == 0", "message": "所有步骤应成功"},
      {"type": "expression", "expr": "max(step_2_1.elapsed_ms) < 100", "message": "每次写入延迟 < 100ms"}
    ],
    
    "cleanup": {
      "steps": [
        {"id": "c1", "type": "SetParameter", "paramId": 65541, "value": 0, "description": "复位目标转速"}
      ]
    }
  }
}
```

---

## 步骤组合模式

### 模式 1: 序列模式 (Sequence)

```
Step1 → Step2 → Step3 → ... → StepN
```

最基本的模式，步骤按顺序执行。适用于线性测试流程。

### 模式 2: 循环模式 (Loop)

```
Loop(count=N) {
    StepA → StepB → StepC
}
```

重复执行内部步骤 N 次。支持 `loop.index`（0-based）和 `loop.iteration`（1-based）变量。

```json
{
  "type": "Loop",
  "count": 10,
  "steps": [
    {"type": "SetParameter", "paramId": 65541, "value": "${loop.index * 100}"},
    {"type": "Delay", "duration_ms": 200}
  ]
}
```

### 模式 3: 条件循环 (While)

```
While(condition) {
    StepA → StepB
}
```

```json
{
  "type": "Loop",
  "while": "MotorSpeed < 3000",
  "max_iterations": 100,
  "steps": [
    {"type": "SetParameter", "paramId": 65541, "value": "${MotorSpeed + 100}"},
    {"type": "Delay", "duration_ms": 200}
  ]
}
```

### 模式 4: 分支模式 (Branch)

```
if (condition) {
    StepA
} else {
    StepB
}
```

```json
{
  "type": "Branch",
  "condition": "MotorTemperature > 60",
  "then": [
    {"type": "SendCommand", "command": "ReducePower"}
  ],
  "else": [
    {"type": "SendCommand", "command": "NormalOperation"}
  ]
}
```

### 模式 5: 测-写-验模式 (Measure-Write-Verify)

```
SetParameter → WaitFor → AssertCondition
```

参数配置测试的标准模式：
1. 写入参数值
2. 等待条件满足
3. 断言验证

### 模式 6: 子测试模式 (SubTest)

```
SubTest("common/startup.json", params={...})
```

```json
{
  "type": "SubTest",
  "testRef": "common/motor_startup.json",
  "params": {
    "target_speed": 1500,
    "acceleration": 300
  }
}
```

### 模式 7: 数据采集模式 (Record-Then-Analyze)

```
RecordData(continuous=true) → [执行操作] → RecordData(continuous=false) → AssertCondition(data)
```

---

## 变量作用域

### 作用域层次

```
┌─────────────────────────────────────────────┐
│  Global Scope (系统级)                       │
│  - 所有测试用例共享的全局变量                │
│  - 如: device_model, firmware_version       │
├─────────────────────────────────────────────┤
│  Test Scope (测试级)                         │
│  - 当前测试用例内的变量                      │
│  - 如: setup 中设置的变量                    │
├─────────────────────────────────────────────┤
│  Block Scope (块级)                          │
│  - Loop / Branch / SubTest 内部变量          │
│  - 如: loop.index, subtest 参数             │
└─────────────────────────────────────────────┘
```

### 变量引用语法

```
${variable_name}            — 引用当前作用域变量
${MotorSpeed}               — 引用 MotorSpeed 参数当前值
${FaultStatus.OverCurrent}  — 引用 BitField 的指定位
${loop.index}               — 循环索引 (0-based)
${loop.iteration}           — 循环迭代次数 (1-based)
${param.xxx}                — SubTest 传入参数
${step_1.result}            — 引用步骤结果
${step_1.elapsed_ms}        — 引用步骤耗时
```

### 变量解析规则

1. 变量解析从当前作用域开始，向上搜索父作用域
2. 参数值（如 `MotorSpeed`）从 `TestContext` 的 `paramManager` 读取最新值
3. 变量名大小写敏感
4. 未解析的变量返回空字符串并记录警告

---

## 断言表达式语法

### 表达式引擎

使用 `exprtk` 兼容的表达式语法，支持以下运算符和函数：

### 运算符

| 类别 | 运算符 | 说明 |
|------|--------|------|
| 算术 | `+`, `-`, `*`, `/`, `%`, `^` | 基本算术 |
| 比较 | `==`, `!=`, `<`, `>`, `<=`, `>=` | 比较运算 |
| 逻辑 | `&&`, `\|\|`, `!` | 逻辑运算 |
| 位运算 | `&`, `\|`, `^`, `~`, `<<`, `>>` | 位运算 |

### 内置函数

| 函数 | 说明 |
|------|------|
| `abs(x)` | 绝对值 |
| `min(x, y)` | 最小值 |
| `max(x, y)` | 最大值 |
| `clamp(x, lo, hi)` | 限制范围 |
| `round(x)` | 四舍五入 |
| `isnan(x)` | 是否为 NaN |
| `count(results, 'failed')` | 统计失败步骤数 |
| `count(results, 'passed')` | 统计通过步骤数 |
| `avg(series)` | 时间序列平均值 |
| `max(series)` | 时间序列最大值 |
| `min(series)` | 时间序列最小值 |
| `stddev(series)` | 时间序列标准差 |

### 断言类型

| 类型 | 配置 | 说明 |
|------|------|------|
| `value_in_range` | `paramId`, `min`, `max` | 参数值在范围内 |
| `value_equals` | `paramId`, `expected` | 参数值等于预期值 |
| `expression` | `expr` | 自定义表达式 |
| `no_fault` | (无) | 无故障标志 |
| `time_series` | `paramId`, `max_overshoot`, `settling_time` | 时间序列分析 |

---

## Loop 和 Branch 语义

### Loop 语义

```
Loop 执行流程:
  1. 创建新的 Block Scope
  2. 设置 loop.index = 0, loop.iteration = 1
  3. 执行内部步骤
  4. 检查内部步骤结果:
     - 如果 continueOnFailure=true 且步骤失败: 记录失败, 继续循环
     - 如果 continueOnFailure=false 且步骤失败: 退出循环, 返回失败
     - 如果所有步骤成功: loop.index++, loop.iteration++
  5. 检查循环条件:
     - count 模式: 迭代次数 < count
     - while 模式: 条件为 true 且 迭代次数 < max_iterations
  6. 循环结束: 弹出 Block Scope
```

### Branch 语义

```
Branch 执行流程:
  1. 评估 condition 表达式
  2. 创建新的 Block Scope
  3. 如果 condition 为 true:
     - 执行 then 分支步骤
  4. 如果 condition 为 false:
     - 执行 else 分支步骤 (如果存在)
  5. 弹出 Block Scope
  6. 返回分支执行结果
```

---

## SubTest 调用约定

### 调用语法

```json
{
  "type": "SubTest",
  "testRef": "path/to/test.json",
  "params": {
    "target_speed": 3000,
    "timeout_ms": 10000
  }
}
```

### 参数传递

子测试通过 `params` 接收参数，在子测试内部通过 `${param.xxx}` 访问：

```json
// 子测试: common/motor_startup.json
{
  "test": {
    "id": "motor_startup",
    "steps": [
      {"type": "SetParameter", "paramId": 65541, "value": "${param.target_speed}"},
      {"type": "WaitFor", "condition": "MotorSpeed >= ${param.target_speed} * 0.95", 
       "timeout_ms": "${param.timeout_ms}"}
    ]
  }
}
```

### 结果传递

子测试的 `TestReport` 被合并到父测试报告中：
- 子测试步骤以 `[subtest_name] step_id` 格式展平到父测试步骤列表
- 子测试的 `assertions` 合并到父测试断言列表
- 子测试的 `recorded_data` 合并到父测试数据记录

---

## 测试数据记录

### 记录什么

| 数据类别 | 说明 | 示例 |
|----------|------|------|
| 参数时序数据 | 指定参数在测试期间的时间序列 | `[(t, MotorSpeed), ...]` |
| 步骤执行结果 | 每个步骤的状态、耗时 | `{step_id, status, elapsed_ms}` |
| 断言结果 | 每个断言的通过/失败 | `{assertion, passed, actual, expected}` |
| 系统事件 | 故障、告警、状态变更 | `{timestamp, event_type, detail}` |
| 环境信息 | 设备型号、固件版本、软件版本 | `{device, fw_ver, sw_ver}` |

### 何时记录

```
RecordData 步骤控制记录启停:

  RecordData(continuous=true)   ← 开始记录
  [执行操作步骤...]
  RecordData(continuous=false)  ← 停止记录

或使用持续时间模式:

  RecordData(duration_ms=5000)  ← 记录 5 秒后自动停止
```

### 数据存储格式

```json
{
  "data_series": [
    {
      "label": "MotorSpeed",
      "paramId": 65536,
      "unit": "RPM",
      "sample_rate_hz": 100,
      "data_points": [
        {"t": 0.0, "v": 0},
        {"t": 0.01, "v": 6},
        {"t": 0.02, "v": 12}
      ]
    }
  ]
}
```

---

## 后续实现注意事项

1. **JSON Schema 校验**：`TestLoader` 加载 JSON 后，使用 JSON Schema 验证器（如 `valijson`）校验格式完整性。校验失败时返回具体错误位置和原因。

2. **变量表达式求值时机**：变量在步骤执行时求值，而非加载时。这意味着 `"value": "${MotorSpeed + 100}"` 在 SetParameter 步骤执行时读取当前 MotorSpeed 值计算。

3. **循环变量生命周期**：`loop.index` 和 `loop.iteration` 仅在 Loop 步骤的 Block Scope 内有效。嵌套循环时，内层循环变量遮蔽外层。

4. **SubTest 路径解析**：`testRef` 路径支持相对路径（相对于当前测试文件）和绝对路径（相对于 `tests/` 目录）。路径分隔符统一使用 `/`。

5. **表达式安全性**：`exprtk` 表达式在沙箱中执行，禁用文件 I/O、系统调用等危险函数。表达式执行超时限制为 100ms。

6. **数据记录内存管理**：高频采样（500Hz，10 个参数，持续 60 秒）可能产生 300,000 个数据点。使用环形缓冲区或分块写入临时文件，避免内存溢出。

7. **cleanup 始终执行**：无论测试成功、失败还是超时，`cleanup.steps` 中的步骤必须执行。实现时使用 `try/finally` 或 RAII 模式确保清理。

8. **测试用例版本管理**：测试 JSON 文件纳入 Git 版本控制，`test.version` 字段与 Git tag 对应。报告中记录测试用例版本，确保结果可追溯。

9. **参数化测试（未来 Phase 2）**：支持 `test.params` 数组定义参数组合，引擎自动生成参数矩阵并逐一执行。类似 pytest 的 `@pytest.mark.parametrize`。

10. **测试用例模板**：提供 `templates/` 目录，包含常用测试场景模板（启动测试、斜坡测试、保护测试、通信测试），测试工程师基于模板修改。