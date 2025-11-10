#include "SpecLoader.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariant>

static const char* kEmbeddedSpec = R"JSON(
{
  "groups": ["变电部分", "海上变电部分", "陆上变电部分", "线路部分", "其他设备", "其他费用", 
             "维护费", "停运损失费", "海域租赁费"],
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
        {"name":"unit_price","label":"单价","type":"double","unit":"万元/km","required":false,"defval":0},
        {"name":"length","label":"长度","type":"double","unit":"km","required":false,"defval":0}
      ],
      "formula":"result = unit_price * length;" },

    { "id":"other_equipment", "group":"其他设备", "label":"其他设备", "unit":"万元", "inputs":[
        {"name":"capex","label":"其他设备","type":"double","unit":"万元","required":false,"defval":0}
      ],
      "formula":"result = capex;" },

    { "id":"other_cost", "group":"其他费用", "label":"其他费用", "unit":"万元", "inputs":[
        {"name":"capex","label":"其他费用","type":"double","unit":"万元","required":false,"defval":0}
      ],
      "formula":"result = capex;" },

    { "id":"om_offshore", "group":"维护费", "label":"海上变电设备维护费", "unit":"万元/年", "inputs":[
        {"name":"annual_om_cost","label":"海上变电设备维护费","type":"double","unit":"万元/年","required":false,"defval":0}
      ],
      "formula":"result = annual_om_cost;" },

    { "id":"om_onshore", "group":"维护费", "label":"陆上变电设备维护费", "unit":"万元/年", "inputs":[
        {"name":"annual_om_cost","label":"陆上变电设备维护费","type":"double","unit":"万元/年","required":false,"defval":0}
      ],
      "formula":"result = annual_om_cost;" },

    { "id":"downtime_cable", "group":"停运损失费", "label":"海缆故障停运", "unit":"万元/年", "inputs":[
        {"name":"util_hours","label":"年利用小时数","type":"double","unit":"h","required":false,"defval":0},
        {"name":"length","label":"长度","type":"double","unit":"km","required":false,"defval":0},
        {"name":"terminals","label":"终端数量","type":"int","unit":"个","required":false,"defval":0},
        {"name":"avg_power","label":"年平均功率","type":"double","unit":"MW","required":false,"defval":0},
        {"name":"price","label":"电价","type":"double","unit":"元/kWh","required":false,"defval":0}
      ],
      "formula":"result = util_hours * length * terminals * avg_power * price / 10000.0;" },

    { "id":"downtime_equipment", "group":"停运损失费", "label":"设备故障停运", "unit":"万元/年", "inputs":[
        {"name":"avg_power","label":"年平均功率","type":"double","unit":"MW","required":false,"defval":0}
      ],
      "formula":"result = avg_power;" },

    { "id":"downtime_maintenance", "group":"停运损失费", "label":"设备计划检修停运", "unit":"万元/年", "inputs":[
        {"name":"avg_power","label":"年平均功率","type":"double","unit":"MW","required":false,"defval":0},
        {"name":"maint_hours","label":"维修时长","type":"double","unit":"h","required":false,"defval":0}
      ],
      "formula":"result = avg_power * maint_hours;" },

    { "id":"rent_cable", "group":"海域租赁费", "label":"线路年海域使用费", "unit":"万元/年", "inputs":[
        {"name":"length","label":"长度","type":"double","unit":"km","required":false,"defval":0},
        {"name":"width","label":"宽度","type":"double","unit":"m","required":false,"defval":0},
        {"name":"unit_fee","label":"单位费用","type":"double","unit":"元/m·km/年","required":false,"defval":0}
      ],
      "formula":"result = length * width * unit_fee / 10000.0;" },

    { "id":"rent_equipment", "group":"海域租赁费", "label":"设备年海域使用费", "unit":"万元/年", "inputs":[
        {"name":"area","label":"面积","type":"double","unit":"m²","required":false,"defval":0},
        {"name":"unit_fee","label":"单位费用","type":"double","unit":"元/m²/年","required":false,"defval":0}
      ],
      "formula":"result = area * unit_fee / 10000.0;" }
  ]
}
)JSON";

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
        f.type = io.value("type").toString();
        f.unit = io.value("unit").toString();
        f.required = io.value("required").toBool(false);
        if (io.contains("defval")) f.defval = io.value("defval").toVariant();
        if (io.contains("min")) f.min = io.value("min").toDouble();
        if (io.contains("max")) f.max = io.value("max").toDouble();
        if (io.contains("options")) {
            for (const auto& ov : io.value("options").toArray())
                f.enumOptions << ov.toString();
        }
        s.inputs.push_back(f);
    }
    return s;
}

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

