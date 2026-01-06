/*
 * 规格加载模块实现。
 * 从内嵌 JSON 描述构建 ProjectSpecSet 供界面和计算使用。
 */
#include "SpecLoader.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariant>

/* 内嵌默认规格 JSON，描述所有项目及其输入字段和公式 */
static const char* kEmbeddedSpec = R"JSON(
{
  "groups": ["海上变电部分", "陆上变电部分", "线路部分", "其他设备及其他建筑安装工程", "其他费用及基本预备费",
             "损耗费用", "维护费", "停运损失费", "海域租赁费"],
  "items": [
    { "id":"offshore_converter", "group":"海上变电部分", "label":"海上换流站", "unit":"万元", "inputs":[
        {"name":"capex","label":"海上换流站","type":"double","unit":"万元","required":false,"defval":0}
      ],
      "formula":"result = capex;" },

    { "id":"offshore_compensation", "group":"海上变电部分", "label":"海上无功补偿平台", "unit":"万元", "inputs":[
        {"name":"capex","label":"海上无功补偿平台","type":"double","unit":"万元","required":false,"defval":0}
      ],
      "formula":"result = capex;" },

    { "id":"offshore_substation", "group":"海上变电部分", "label":"海上升压站", "unit":"万元", "inputs":[
        {"name":"capex","label":"海上升压站","type":"double","unit":"万元","required":false,"defval":0}
      ],
      "formula":"result = capex;" },

    { "id":"onshore_converter", "group":"陆上变电部分", "label":"陆上换流站", "unit":"万元", "inputs":[
        {"name":"capex","label":"陆上换流站","type":"double","unit":"万元","required":false,"defval":0}
      ],
      "formula":"result = capex;" },

    { "id":"onshore_control_center", "group":"陆上变电部分", "label":"陆上集控中心", "unit":"万元", "inputs":[
        {"name":"capex","label":"陆上集控中心","type":"double","unit":"万元","required":false,"defval":0}
      ],
      "formula":"result = capex;" },

    { "id":"onshore_compensation", "group":"陆上变电部分", "label":"无功补偿设备", "unit":"万元", "inputs":[
        {"name":"capex","label":"无功补偿设备","type":"double","unit":"万元","required":false,"defval":0}
      ],
      "formula":"result = capex;" },

    { "id":"subsea_cable", "group":"线路部分", "label":"海缆费用", "unit":"万元", "inputs":[
        {"name":"unit_price","label":"本体单价","type":"double","unit":"万元/km","required":false,"defval":0},
        {"name":"unit_price2","label":"施工单价","type":"double","unit":"万元/km","required":false,"defval":0},
        {"name":"length","label":"长度","type":"double","unit":"km","required":false,"defval":0}
      ],
      "formula":"result = (unit_price+unit_price2) * length;" },

    { "id":"other_equipment", "group":"其他设备及其他建筑安装工程", "label":"其他设备及其他建筑安装工程", "unit":"万元", "inputs":[
        {"name":"capex","label":"其他设备及其他建筑安装工程","type":"double","unit":"万元","required":false,"defval":0}
      ],
      "formula":"result = capex;" },

    { "id":"other_cost", "group":"其他费用及基本预备费", "label":"其他费用及基本预备费", "unit":"万元", "inputs":[
        {"name":"rate","label":"折算倍率","type":"double","unit":"倍","required":false,"defval":0.08}
      ],
      "formula":"result = core_capex_total * rate;" },

    { "id":"loss_dc_cable", "group":"损耗费用", "label":"直流海缆损耗", "unit":"万元/年", "inputs":[
        {"name":"resistance","label":"电阻","type":"double","unit":"Ω/km","required":false,"defval":0},
        {"name":"current","label":"电流","type":"double","unit":"A","required":false,"defval":0},
        {"name":"length","label":"长度","type":"double","unit":"km","required":false,"defval":0},
        {"name":"circuits","label":"回路数","type":"int","unit":"回","required":false,"defval":0},
        {"name":"loss_hours","label":"损耗小时数","type":"double","unit":"h","required":false,"defval":0},
        {"name":"price","label":"电价","type":"double","unit":"元/kWh","required":false,"defval":0,"sharedKey":"price"},
        {"name":"override_result","label":"使用设定值","type":"double","unit":"万元/年","required":false}
      ],
      "formula":"result = resistance*current*current*2*length*circuits*loss_hours*price/1000.0/10000.0;" },

    { "id":"loss_ac_cable", "group":"损耗费用", "label":"交流海缆损耗", "unit":"万元/年", "inputs":[
        {"name":"resistance","label":"电阻","type":"double","unit":"Ω/km","required":false,"defval":0},
        {"name":"current","label":"电流","type":"double","unit":"A","required":false,"defval":0},
        {"name":"length","label":"长度","type":"double","unit":"km","required":false,"defval":0},
        {"name":"circuits","label":"回路数","type":"int","unit":"回","required":false,"defval":0},
        {"name":"loss_hours","label":"损耗小时数","type":"double","unit":"h","required":false,"defval":0},
        {"name":"price","label":"电价","type":"double","unit":"元/kWh","required":false,"defval":0,"sharedKey":"price"},
        {"name":"override_result","label":"使用设定值","type":"double","unit":"万元/年","required":false}
      ],
      "formula":"result = resistance*current*current*3*1.55*length*circuits*loss_hours*price/1000.0/10000.0;" },

    { "id":"loss_converter", "group":"损耗费用", "label":"变电/换流损耗", "unit":"万元/年", "inputs":[
        {"name":"transformer_capacity","label":"变压器输送容量","type":"double","unit":"MW","required":false,"defval":0},
        {"name":"valve_capacity","label":"换流阀输送容量","type":"double","unit":"MW","required":false,"defval":0},
        {"name":"loss_hours","label":"损耗小时数","type":"double","unit":"h","required":false,"defval":0},
        {"name":"price","label":"电价","type":"double","unit":"元/kWh","required":false,"defval":0,"sharedKey":"price"},
        {"name":"override_result","label":"使用设定值","type":"double","unit":"万元/年","required":false}
      ],
      "formula":"result = (transformer_capacity*0.003 *2 + valve_capacity*0.008)*loss_hours*price/10.0 ;" },

    { "id":"loss_reactor", "group":"损耗费用", "label":"高抗损耗", "unit":"万元/年", "inputs":[
        {"name":"charge_power","label":"充电功率","type":"double","unit":"Mvar","required":false,"defval":0},
        {"name":"loss_hours","label":"损耗小时数","type":"double","unit":"h","required":false,"defval":0},
        {"name":"price","label":"电价","type":"double","unit":"元/kWh","required":false,"defval":0,"sharedKey":"price"},
        {"name":"override_result","label":"使用设定值","type":"double","unit":"万元/年","required":false}
      ],
      "formula":"result = charge_power*0.6*0.001*loss_hours * price / 10.0;" },

    { "id":"om_substation_equipment", "group":"维护费", "label":"变电设备维护费", "unit":"万元/年", "inputs":[
        {"name":"offshore_rate","label":"海上设备折算倍率","type":"double","unit":"倍","required":false,"defval":0.02},
        {"name":"onshore_rate","label":"陆上设备折算倍率","type":"double","unit":"倍","required":false,"defval":0.02},
        {"name":"override_result","label":"使用设定值","type":"double","unit":"万元/年","required":false}
      ],
      "formula":"result = (offshore_capex_total * offshore_rate) + (onshore_capex_total * onshore_rate);" },

    { "id":"downtime_cable", "group":"停运损失费", "label":"直流海缆故障停运", "unit":"万元/年", "inputs":[
        {"name":"use_hours","label":"年利用小时数","type":"double","unit":"h","required":false,"defval":0},
        {"name":"length","label":"海缆长度","type":"double","unit":"km","required":false,"defval":0},
        {"name":"terminals","label":"终端数量","type":"int","unit":"个","required":false,"defval":0},
        {"name":"avg_power","label":"年平均功率","type":"double","unit":"MW","required":false,"defval":0},
        {"name":"price","label":"电价","type":"double","unit":"元/kWh","required":false,"defval":0,"sharedKey":"price"},
        {"name":"override_result","label":"使用设定值","type":"double","unit":"万元/年","required":false}
      ],
      "formula":"result = (0.03*length/100 + 0.007*terminals/100 + 0.0189)*avg_power*use_hours*0.1808*price/10.0;" },

    { "id":"downtime_cable2", "group":"停运损失费", "label":"交流海缆故障停运", "unit":"万元/年", "inputs":[
        {"name":"use_hours2","label":"年利用小时数","type":"double","unit":"h","required":false,"defval":0},
        {"name":"length2","label":"海缆长度","type":"double","unit":"km","required":false,"defval":0},
        {"name":"terminals2","label":"终端数量","type":"int","unit":"个","required":false,"defval":0},
        {"name":"avg_power2","label":"年平均功率","type":"double","unit":"MW","required":false,"defval":0},
        {"name":"price","label":"电价","type":"double","unit":"元/kWh","required":false,"defval":0,"sharedKey":"price"},
        {"name":"override_result","label":"使用设定值","type":"double","unit":"万元/年","required":false}
      ],
      "formula":"result = (0.03*length2/100 + 0.007*terminals2/100 + 0.0189)*avg_power2*use_hours2*0.1808*price/10.0;" },

    { "id":"downtime_equipment", "group":"停运损失费", "label":"设备故障停运", "unit":"万元/年", "inputs":[
        {"name":"avg_power","label":"年平均功率","type":"double","unit":"MW","required":false,"defval":0},
        {"name":"maintain_hours","label":"故障维修时长","type":"double","unit":"h","required":false,"defval":0},
        {"name":"price","label":"电价","type":"double","unit":"元/kWh","required":false,"defval":0,"sharedKey":"price"},
        {"name":"override_result","label":"使用设定值","type":"double","unit":"万元/年","required":false}
      ],
      "formula":"result = avg_power*maintain_hours*price/10.0 ;" },

    { "id":"downtime_maintenance", "group":"停运损失费", "label":"设备计划检修停运", "unit":"万元/年", "inputs":[
        {"name":"avg_power","label":"年平均功率","type":"double","unit":"MW","required":false,"defval":0},
        {"name":"maint_hours","label":"计划检修时长","type":"double","unit":"h","required":false,"defval":0},
        {"name":"price","label":"电价","type":"double","unit":"元/kWh","required":false,"defval":0,"sharedKey":"price"},
        {"name":"override_result","label":"使用设定值","type":"double","unit":"万元/年","required":false}
      ],
      "formula":"result = avg_power*maint_hours*price/10.0;" },

    { "id":"rent_cable1", "group":"海域租赁费", "label":"直流海缆年海域使用费", "unit":"万元/年", "inputs":[
        {"name":"dc_length","label":"直流海缆长度","type":"double","unit":"km","required":false,"defval":0},
        {"name":"dc_width","label":"直流用海宽度","type":"double","unit":"m","required":false,"defval":0},
        {"name":"unit_fee","label":"单公顷费用","type":"double","unit":"万元/年","required":false,"defval":0},
        {"name":"override_result","label":"使用设定值","type":"double","unit":"万元/年","required":false}
      ],
      "formula":"result = dc_length*dc_width * unit_fee / 10.0;" },

    { "id":"rent_cable2", "group":"海域租赁费", "label":"交流海缆年海域使用费", "unit":"万元/年", "inputs":[
        {"name":"ac_length","label":"交流海缆长度","type":"double","unit":"km","required":false,"defval":0},
        {"name":"ac_width","label":"交流用海宽度","type":"double","unit":"m","required":false,"defval":0},
        {"name":"unit_fee","label":"单公顷费用","type":"double","unit":"万元/年","required":false,"defval":0},
        {"name":"override_result","label":"使用设定值","type":"double","unit":"万元/年","required":false}
      ],
      "formula":"result = ac_length*ac_width * unit_fee / 10.0;" },
    
    { "id":"rent_equipment", "group":"海域租赁费", "label":"平台年海域使用费", "unit":"万元/年", "inputs":[
        {"name":"area","label":"面积","type":"double","unit":"m²","required":false,"defval":0},
        {"name":"unit_fee","label":"单公顷费用","type":"double","unit":"万元/年","required":false,"defval":0},
        {"name":"override_result","label":"使用设定值","type":"double","unit":"万元/年","required":false}
      ],
      "formula":"result = area * unit_fee / 10000.0;" }
  ]
}
 )JSON";

// 从单个 JSON 对象解析出 ProjectSpec 以及其输入字段列表
static ProjectSpec parseItem(const QJsonObject& o) {
    ProjectSpec s;
    s.id = o.value("id").toString();
    s.group = o.value("group").toString();
    s.label = o.value("label").toString();
    s.unit  = o.value("unit").toString();
    s.formula = o.value("formula").toString();
    s.note = o.value("note").toString();
    s.groupHeader = o.value("groupHeader").toBool(false);

    const auto arr = o.value("inputs").toArray();
    for (const auto& v : arr) {
        const auto io = v.toObject();
        InputField f;
        f.name = io.value("name").toString();
        f.label = io.value("label").toString();
        f.unit = io.value("unit").toString();
        f.sharedKey = io.value("sharedKey").toString();
        f.required = io.value("required").toBool(false);
        if (io.contains("defval")) f.defval = io.value("defval").toVariant();
        if (io.contains("min")) f.min = io.value("min").toDouble();
        if (io.contains("max")) f.max = io.value("max").toDouble();
        s.inputs.push_back(f);
    }
    return s;
}

// 加载内嵌的默认规格，填充分组顺序、项目列表及分组到行索引的映射
ProjectSpecSet SpecLoader::loadDefault() {
    ProjectSpecSet set;
    const auto doc = QJsonDocument::fromJson(QByteArray(kEmbeddedSpec));
    const auto obj = doc.object();
    for (const auto& v : obj.value("groups").toArray())
        set.groupsInOrder << v.toString();

    const auto items = obj.value("items").toArray();
    for (const auto& v : items) {
        set.items.push_back(parseItem(v.toObject()));
    }
    for (int i=0;i<set.items.size();++i) {
        set.groupRows[ set.items[i].group ].push_back(i);
    }
    return set;
}
