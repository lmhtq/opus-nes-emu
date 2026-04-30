# MOD-XXX: [模块名称]

## 元数据 (Metadata)

- **ID**: MOD-XXX
- **关联概要设计 (Related Overview)**: OVERVIEW-001
- **关联需求 (Related Requirements)**: REQ-XXX
- **状态 (Status)**: Draft | Review | Approved
- **创建日期 (Created)**: YYYY-MM-DD
- **最后更新 (Updated)**: YYYY-MM-DD

## 功能职责 (Responsibilities)

[What this module does, in Chinese. 用中文描述模块职责。]

## 接口设计 (Interface Design)

```cpp
// include/fcemu/xxx.h
namespace fcemu {

class Xxx {
public:
    Xxx();
    void init();
    // ...
};

}
```

## 依赖关系 (Dependencies)

| 依赖模块 | 说明 |
|----------|------|
| MOD-YYY | 本模块依赖 YYY |
| MOD-ZZZ | 本模块被 ZZZ 依赖 |

## 数据结构 (Data Structures)

[Key data structures used in this module. 列出核心数据结构。]

## 状态机 (State Machines)

[If applicable, describe state machines. 如有状态机，描述状态和转换。]

## 关联硬件文档 (Related Hardware Docs)

- `docs/hardware/cpu/registers.md` （引用具体章节）
- ...

## 变更记录 (Change History)

- YYYY-MM-DD: Initial version
