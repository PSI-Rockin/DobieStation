#include <QtWidgets/QMessageBox>
#include "breakpoint_window.hpp"
#include "ui_breakpoint_window.h"



breakpoint_window::breakpoint_window(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::breakpoint_window)
{
    ui->setupUi(this);
}

breakpoint_window::~breakpoint_window()
{
    delete ui;
}

DebuggerBreakpoint* breakpoint_window::make_pc_breakpoint()
{
    setWindowTitle("PC Breakpoint");
    ui->comparison_combo->hide();
    ui->comparison_label->hide();
    ui->reg_type_label->hide();
    ui->reg_type_combo->hide();
    ui->reg_label->hide();
    ui->reg_combo->hide();
    ui->data_type_combo->hide();
    ui->data_type_label->hide();
    ui->value_label->hide();
    ui->value_line->hide();

    this->exec();

    if(this->result() == QDialog::Rejected)
        return nullptr;


    uint32_t addr = get_address();
    if(addr & 0x3)
    {
        QMessageBox box;
        box.critical(0,"Error", "address is not aligned!");
        return nullptr;
    }
    return new EmotionBreakpoints::PC(addr, ui->clear_checkbox->isChecked());
}



uint32_t breakpoint_window::get_address()
{
    QString addr_str = ui->address_line->text();
    char* charp = addr_str.toUtf8().data();
    return strtoul(charp, nullptr, 16);
}

template<>
float breakpoint_window::get_value<float>()
{
    return ui->value_line->text().toFloat();
}

template<>
uint128_t breakpoint_window::get_value<uint128_t>()
{
    QString value_str = ui->value_line->text();
    QString lo = value_str.right(16);
    uint128_t r;
    r.lo = strtoull(lo.toUtf8().data(), nullptr, 16);
    r.hi = 0;
    int32_t hi_len = value_str.length() - lo.length();
    if(hi_len)
    {
        QString hi = value_str.left(hi_len);
        r.hi = strtoull(hi.toUtf8().data(), nullptr, 16);
    }
    //printf("%s -> 0x%08lx%08lx\n", value_str.toUtf8().data(), r.hi, r.lo);
    return r;
}

template<typename T>
T breakpoint_window::get_value()
{
    QString value_str = ui->value_line->text();
    char* charp = value_str.toUtf8().data();
    uint64_t v = strtoull(charp, nullptr, 16);

    // check to see if it overflows:
    T r = (T)v;

    if(r != v)
    {
        QMessageBox messageBox;
        messageBox.critical(0,"Error","Overflow of breakpoint value");
    }

    return r;
}

template uint8_t breakpoint_window::get_value();
template uint16_t breakpoint_window::get_value();
template uint32_t breakpoint_window::get_value();
template uint64_t breakpoint_window::get_value();

DebuggerBreakpoint* breakpoint_window::make_memory_breakpoint(uint32_t addr, bool limit_to_32)
{
    char buff[64];
    sprintf(buff, "0x%x", addr);
    ui->address_line->setText(QString(buff));
    make_memory_breakpoint(limit_to_32);
}

DebuggerBreakpoint *breakpoint_window::make_register_breakpoint(std::vector<EERegisterDefinition>* available_regs)
{
    ee_regs = available_regs;
    setWindowTitle("Register Breakpoint");
    ui->address_line->hide();
    ui->address_label->hide();
    setup_comparison_combo_boxes();

    // set up the register combos
    for(auto name : ee_register_kind_names)
        ui->reg_type_combo->addItem(QString(name));

    this->exec();

    if(this->result() == QDialog::Rejected || !register_selected)
        return nullptr;

    auto data_type = (EmotionBreakpoints::MemoryComparisonType)ui->data_type_combo->currentIndex();
    auto data_kind = (EmotionBreakpoints::MemoryComparisonKind)ui->comparison_combo->currentIndex();
    bool clear = ui->clear_checkbox->isChecked();
    auto reg_def = (*available_regs)[combo_reg_idx[ui->reg_combo->currentIndex()]];

    return make_register_breakpoint(reg_def, data_type, data_kind, clear);
}

DebuggerBreakpoint *breakpoint_window::make_register_breakpoint(std::vector<IOPRegisterDefinition>* available_regs)
{
    iop_regs = available_regs;
    setWindowTitle("Register Breakpoint");
    ui->address_line->hide();
    ui->address_label->hide();
    setup_comparison_combo_boxes(true);

    // set up the register combos
    for(auto name : iop_register_kind_names)
        ui->reg_type_combo->addItem(QString(name));

    this->exec();

    if(this->result() == QDialog::Rejected || !register_selected)
        return nullptr;

    auto data_type = (EmotionBreakpoints::MemoryComparisonType)ui->data_type_combo->currentIndex();
    auto data_kind = (EmotionBreakpoints::MemoryComparisonKind)ui->comparison_combo->currentIndex();
    bool clear = ui->clear_checkbox->isChecked();
    auto reg_def = (*available_regs)[combo_reg_idx[ui->reg_combo->currentIndex()]];

    return make_register_breakpoint(reg_def, data_type, data_kind, clear);
}

DebuggerBreakpoint *breakpoint_window::make_register_breakpoint(EERegisterDefinition &def)
{
    setWindowTitle("Register Breakpoint");
    ui->address_line->hide();
    ui->address_label->hide();
    ui->reg_combo->hide();
    ui->reg_type_combo->hide();
    ui->reg_type_label->hide();
    ui->reg_label->setText("Register: " + QString(def.get_name().c_str()));
    setup_comparison_combo_boxes();

    this->exec();

    if(this->result() == QDialog::Rejected)
        return nullptr;

    auto data_type = (EmotionBreakpoints::MemoryComparisonType)ui->data_type_combo->currentIndex();
    auto data_kind = (EmotionBreakpoints::MemoryComparisonKind)ui->comparison_combo->currentIndex();
    bool clear = ui->clear_checkbox->isChecked();

    return make_register_breakpoint(def, data_type, data_kind, clear);
}

DebuggerBreakpoint *breakpoint_window::make_register_breakpoint(IOPRegisterDefinition &def)
{
    setWindowTitle("Register Breakpoint");
    ui->address_line->hide();
    ui->address_label->hide();
    ui->reg_combo->hide();
    ui->reg_type_combo->hide();
    ui->reg_type_label->hide();
    ui->reg_label->setText("Register: " + QString(def.get_name().c_str()));
    setup_comparison_combo_boxes(true);

    this->exec();

    if(this->result() == QDialog::Rejected)
        return nullptr;

    auto data_type = (EmotionBreakpoints::MemoryComparisonType)ui->data_type_combo->currentIndex();
    auto data_kind = (EmotionBreakpoints::MemoryComparisonKind)ui->comparison_combo->currentIndex();
    bool clear = ui->clear_checkbox->isChecked();

    return make_register_breakpoint(def, data_type, data_kind, clear);
}

DebuggerBreakpoint* breakpoint_window::make_register_breakpoint(EERegisterDefinition &def,
                                                               EmotionBreakpoints::MemoryComparisonType type,
                                                               EmotionBreakpoints::MemoryComparisonKind compare_kind,
                                                               bool clear)
{
    switch(type)
    {
        case EmotionBreakpoints::MemoryComparisonType::FLOAT:
        {
            float v = get_value<float>();
            return new EmotionBreakpoints::Register<float>(def.kind, def.id, v, type, compare_kind, clear);
        }

        case EmotionBreakpoints::MemoryComparisonType::U8:
        {
            uint8_t v = get_value<uint8_t>();
            return new EmotionBreakpoints::Register<uint8_t>(def.kind, def.id, v, type, compare_kind, clear);
        }

        case EmotionBreakpoints::MemoryComparisonType::U16:
        {
            uint16_t v = get_value<uint16_t>();
            return new EmotionBreakpoints::Register<uint16_t>(def.kind, def.id, v, type, compare_kind, clear);
        }

        case EmotionBreakpoints::MemoryComparisonType::U32:
        {
            uint32_t v = get_value<uint32_t>();
            return new EmotionBreakpoints::Register<uint32_t>(def.kind, def.id, v, type, compare_kind, clear);
        }

        case EmotionBreakpoints::MemoryComparisonType::U64:
        {
            uint64_t v = get_value<uint64_t>();
            return new EmotionBreakpoints::Register<uint64_t>(def.kind, def.id, v, type, compare_kind, clear);
        }

        case EmotionBreakpoints::MemoryComparisonType::U128:
        {
            uint128_t v = get_value<uint128_t>();
            return new EmotionBreakpoints::Register<uint128_t>(def.kind, def.id, v, type, compare_kind, clear);
        }
    }
}

DebuggerBreakpoint* breakpoint_window::make_register_breakpoint(IOPRegisterDefinition &def,
                                                                EmotionBreakpoints::MemoryComparisonType type,
                                                                EmotionBreakpoints::MemoryComparisonKind compare_kind,
                                                                bool clear)
{
    switch(type)
    {
        case EmotionBreakpoints::MemoryComparisonType::FLOAT:
        {
            float v = get_value<float>();
            return new EmotionBreakpoints::Register<float>(def.kind, def.id, v, type, compare_kind, clear);
        }

        case EmotionBreakpoints::MemoryComparisonType::U8:
        {
            uint8_t v = get_value<uint8_t>();
            return new EmotionBreakpoints::Register<uint8_t>(def.kind, def.id, v, type, compare_kind, clear);
        }

        case EmotionBreakpoints::MemoryComparisonType::U16:
        {
            uint16_t v = get_value<uint16_t>();
            return new EmotionBreakpoints::Register<uint16_t>(def.kind, def.id, v, type, compare_kind, clear);
        }

        case EmotionBreakpoints::MemoryComparisonType::U32:
        {
            uint32_t v = get_value<uint32_t>();
            return new EmotionBreakpoints::Register<uint32_t>(def.kind, def.id, v, type, compare_kind, clear);
        }

        default:
            Errors::die("unknown register breakpoint type");
    }
}

DebuggerBreakpoint* breakpoint_window::make_memory_breakpoint(bool limit_to_32)
{
    setWindowTitle("Memory Breakpoint");
    ui->reg_type_label->hide();
    ui->reg_type_combo->hide();
    ui->reg_label->hide();
    ui->reg_combo->hide();
    setup_comparison_combo_boxes(limit_to_32);

    this->exec();

    if(this->result() == QDialog::Rejected)
        return nullptr;

    auto data_type = (EmotionBreakpoints::MemoryComparisonType)ui->data_type_combo->currentIndex();
    auto data_kind = (EmotionBreakpoints::MemoryComparisonKind)ui->comparison_combo->currentIndex();
    bool clear = ui->clear_checkbox->isChecked();
    switch(data_type)
    {
        case EmotionBreakpoints::MemoryComparisonType::FLOAT:
        {
            float v = get_value<float>();
            uint32_t addr = get_address();
            if(addr & 3)
            {
                QMessageBox box;
                box.critical(0,"Error", "address is not aligned!");
                box.exec();
                return nullptr;
            }
            return new EmotionBreakpoints::Memory<float>(addr, v, data_type, data_kind, clear);
        }

        case EmotionBreakpoints::MemoryComparisonType::U8:
        {
            uint8_t v = get_value<uint8_t>();
            return new EmotionBreakpoints::Memory<uint8_t>(get_address(), v, data_type, data_kind, clear);
        }

        case EmotionBreakpoints::MemoryComparisonType::U16:
        {
            uint16_t v = get_value<uint16_t>();
            uint32_t addr = get_address();
            if(addr & 1)
            {
                QMessageBox box;
                box.critical(0,"Error", "address is not aligned!");
                return nullptr;
            }
            return new EmotionBreakpoints::Memory<uint16_t>(addr, v, data_type, data_kind, clear);
        }

        case EmotionBreakpoints::MemoryComparisonType::U32:
        {
            uint32_t v = get_value<uint32_t>();
            uint32_t addr = get_address();
            if(addr & 3)
            {
                QMessageBox box;
                box.critical(0,"Error", "address is not aligned!");
                return nullptr;
            }
            return new EmotionBreakpoints::Memory<uint32_t>(addr, v, data_type, data_kind, clear);
        }

        case EmotionBreakpoints::MemoryComparisonType::U64:
        {
            uint64_t v = get_value<uint64_t>();
            uint32_t addr = get_address();
            if(addr & 7)
            {
                QMessageBox box;
                box.critical(0,"Error", "address is not aligned!");
                return nullptr;
            }
            return new EmotionBreakpoints::Memory<uint64_t>(addr, v, data_type, data_kind, clear);
        }

        case EmotionBreakpoints::MemoryComparisonType::U128:
        {
            uint128_t v = get_value<uint128_t>();
            uint32_t addr = get_address();
            if(addr & 15)
            {
                QMessageBox box;
                box.critical(0,"Error", "address is not aligned!");
                return nullptr;
            }
            return new EmotionBreakpoints::Memory<uint128_t>(addr, v, data_type, data_kind, clear);
        }
    }
}


void breakpoint_window::setup_comparison_combo_boxes(bool limit_to_32)
{
    for(auto name : mem_compare_kind_names)
        ui->comparison_combo->addItem(QString(name));

    if(limit_to_32)
    {
        for(uint32_t i = 0; i < (uint32_t)EmotionBreakpoints::MemoryComparisonType::U64; i++)
            ui->data_type_combo->addItem(QString(mem_compare_type_names[i]));
    }
    else
    {
        for(auto name : mem_compare_type_names)
            ui->data_type_combo->addItem(QString(name));
    }

}

void breakpoint_window::on_reg_type_combo_currentIndexChanged(int index)
{
    ui->reg_combo->clear();
    combo_reg_idx.clear();

    if(ee_regs)
    {
        auto kind = (EERegisterKind)index;
        for(uint32_t i = 0; i < ee_regs->size(); i++)
        {
            auto& reg = (*ee_regs)[i];
            if(reg.kind == kind)
            {
                ui->reg_combo->addItem(reg.get_name().c_str());
                combo_reg_idx.push_back(i);
            }

        }
        register_selected = true;
    }
    else if(iop_regs)
    {
        auto kind = (IOPRegisterKind)index;
        for(uint32_t i = 0; i < iop_regs->size(); i++)
        {
            auto& reg = (*iop_regs)[i];
            if(reg.kind == kind)
            {
                ui->reg_combo->addItem(reg.get_name().c_str());
                combo_reg_idx.push_back(i);
            }

        }
        register_selected = true;
    }
}


