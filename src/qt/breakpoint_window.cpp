#include <QtWidgets/QMessageBox>
#include "breakpoint_window.hpp"

breakpoint_window::breakpoint_window(QWidget *parent) :
    QDialog(parent)
{
    build_ui();
}

breakpoint_window::~breakpoint_window()
{

}

void breakpoint_window::build_ui()
{
    resize(324, 279);
    grid_layout = new QGridLayout(this);
    
    reg_label = new QLabel(this);
    grid_layout->addWidget(reg_label, 5, 0, 1, 1);

    reg_type_combo = new QComboBox(this);
    grid_layout->addWidget(reg_type_combo, 4, 2, 1, 1);

    address_label = new QLabel(this);
    grid_layout->addWidget(address_label, 0, 0, 1, 1);

    clear_checkbox = new QCheckBox(this);
    grid_layout->addWidget(clear_checkbox, 6, 2, 1, 1);

    reg_type_label = new QLabel(this);
    grid_layout->addWidget(reg_type_label, 4, 0, 1, 1);

    verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);
    grid_layout->addItem(verticalSpacer, 7, 2, 1, 1);

    ok_cancel_button = new QDialogButtonBox(this);
    ok_cancel_button->setOrientation(Qt::Horizontal);
    ok_cancel_button->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);
    grid_layout->addWidget(ok_cancel_button, 8, 2, 1, 5);

    address_line = new QLineEdit(this);
    grid_layout->addWidget(address_line, 0, 2, 1, 1);

    comparison_combo = new QComboBox(this);
    grid_layout->addWidget(comparison_combo, 2, 2, 1, 1);

    value_label = new QLabel(this);
    grid_layout->addWidget(value_label, 3, 0, 1, 1);

    comparison_label = new QLabel(this);
    grid_layout->addWidget(comparison_label, 2, 0, 1, 1);

    data_type_combo = new QComboBox(this);
    grid_layout->addWidget(data_type_combo, 1, 2, 1, 1);

    data_type_label = new QLabel(this);
    grid_layout->addWidget(data_type_label, 1, 0, 1, 1);

    value_line = new QLineEdit(this);
    grid_layout->addWidget(value_line, 3, 2, 1, 1);

    reg_combo = new QComboBox(this);
    grid_layout->addWidget(reg_combo, 5, 2, 1, 1);
    
    reg_label->setText("Register");
    address_label->setText("Address");
    clear_checkbox->setText("Clear after hit?");
    reg_type_label->setText("Register Type");
    value_label->setText("Value");
    comparison_label->setText("Comparison");
    data_type_label->setText("Data Type");
    connect(ok_cancel_button, SIGNAL(accepted()), this, SLOT(accept()));
    connect(ok_cancel_button, SIGNAL(rejected()), this, SLOT(reject()));
    connect(reg_type_combo, SIGNAL(activated(int)), this, SLOT(on_reg_type_combo_currentIndexChanged(int)));
    on_reg_type_combo_currentIndexChanged(0); // populate list initially
}

DebuggerBreakpoint* breakpoint_window::make_pc_breakpoint()
{
    setWindowTitle("PC Breakpoint");
    comparison_combo->hide();
    comparison_label->hide();
    reg_type_label->hide();
    reg_type_combo->hide();
    reg_label->hide();
    reg_combo->hide();
    data_type_combo->hide();
    data_type_label->hide();
    value_label->hide();
    value_line->hide();

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
    return new EmotionBreakpoints::PC(addr, clear_checkbox->isChecked());
}



uint32_t breakpoint_window::get_address()
{
    QString addr_str = address_line->text();
    char* charp = addr_str.toUtf8().data();
    return strtoul(charp, nullptr, 16);
}

template<>
float breakpoint_window::get_value<float>()
{
    return value_line->text().toFloat();
}

template<>
uint128_t breakpoint_window::get_value<uint128_t>()
{
    QString value_str = value_line->text();
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
    QString value_str = value_line->text();
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
    address_line->setText(QString(buff));
    make_memory_breakpoint(limit_to_32);
}

DebuggerBreakpoint *breakpoint_window::make_register_breakpoint(std::vector<EERegisterDefinition>* available_regs)
{
    ee_regs = available_regs;
    setWindowTitle("Register Breakpoint");
    address_line->hide();
    address_label->hide();
    setup_comparison_combo_boxes();

    // set up the register combos
    for(auto name : ee_register_kind_names)
        reg_type_combo->addItem(QString(name));

    this->exec();

    if(this->result() == QDialog::Rejected || !register_selected)
        return nullptr;

    auto data_type = (EmotionBreakpoints::MemoryComparisonType)data_type_combo->currentIndex();
    auto data_kind = (EmotionBreakpoints::MemoryComparisonKind)comparison_combo->currentIndex();
    bool clear = clear_checkbox->isChecked();
    auto reg_def = (*available_regs)[combo_reg_idx[reg_combo->currentIndex()]];

    return make_register_breakpoint(reg_def, data_type, data_kind, clear);
}

DebuggerBreakpoint *breakpoint_window::make_register_breakpoint(std::vector<IOPRegisterDefinition>* available_regs)
{
    iop_regs = available_regs;
    setWindowTitle("Register Breakpoint");
    address_line->hide();
    address_label->hide();
    setup_comparison_combo_boxes(true);

    // set up the register combos
    for(auto name : iop_register_kind_names)
        reg_type_combo->addItem(QString(name));

    this->exec();

    if(this->result() == QDialog::Rejected || !register_selected)
        return nullptr;

    auto data_type = (EmotionBreakpoints::MemoryComparisonType)data_type_combo->currentIndex();
    auto data_kind = (EmotionBreakpoints::MemoryComparisonKind)comparison_combo->currentIndex();
    bool clear = clear_checkbox->isChecked();
    auto reg_def = (*available_regs)[combo_reg_idx[reg_combo->currentIndex()]];

    return make_register_breakpoint(reg_def, data_type, data_kind, clear);
}

DebuggerBreakpoint *breakpoint_window::make_register_breakpoint(EERegisterDefinition &def)
{
    setWindowTitle("Register Breakpoint");
    address_line->hide();
    address_label->hide();
    reg_combo->hide();
    reg_type_combo->hide();
    reg_type_label->hide();
    reg_label->setText("Register: " + QString(def.get_name().c_str()));
    setup_comparison_combo_boxes();

    this->exec();

    if(this->result() == QDialog::Rejected)
        return nullptr;

    auto data_type = (EmotionBreakpoints::MemoryComparisonType)data_type_combo->currentIndex();
    auto data_kind = (EmotionBreakpoints::MemoryComparisonKind)comparison_combo->currentIndex();
    bool clear = clear_checkbox->isChecked();

    return make_register_breakpoint(def, data_type, data_kind, clear);
}

DebuggerBreakpoint *breakpoint_window::make_register_breakpoint(IOPRegisterDefinition &def)
{
    setWindowTitle("Register Breakpoint");
    address_line->hide();
    address_label->hide();
    reg_combo->hide();
    reg_type_combo->hide();
    reg_type_label->hide();
    reg_label->setText("Register: " + QString(def.get_name().c_str()));
    setup_comparison_combo_boxes(true);

    this->exec();

    if(this->result() == QDialog::Rejected)
        return nullptr;

    auto data_type = (EmotionBreakpoints::MemoryComparisonType)data_type_combo->currentIndex();
    auto data_kind = (EmotionBreakpoints::MemoryComparisonKind)comparison_combo->currentIndex();
    bool clear = clear_checkbox->isChecked();

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
    reg_type_label->hide();
    reg_type_combo->hide();
    reg_label->hide();
    reg_combo->hide();
    setup_comparison_combo_boxes(limit_to_32);

    this->exec();

    if(this->result() == QDialog::Rejected)
        return nullptr;

    auto data_type = (EmotionBreakpoints::MemoryComparisonType)data_type_combo->currentIndex();
    auto data_kind = (EmotionBreakpoints::MemoryComparisonKind)comparison_combo->currentIndex();
    bool clear = clear_checkbox->isChecked();
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
        comparison_combo->addItem(QString(name));

    if(limit_to_32)
    {
        for(uint32_t i = 0; i < (uint32_t)EmotionBreakpoints::MemoryComparisonType::U64; i++)
            data_type_combo->addItem(QString(mem_compare_type_names[i]));
    }
    else
    {
        for(auto name : mem_compare_type_names)
            data_type_combo->addItem(QString(name));
    }

}

void breakpoint_window::on_reg_type_combo_currentIndexChanged(int index)
{
    reg_combo->clear();
    combo_reg_idx.clear();

    if(ee_regs)
    {
        auto kind = (EERegisterKind)index;
        for(uint32_t i = 0; i < ee_regs->size(); i++)
        {
            auto& reg = (*ee_regs)[i];
            if(reg.kind == kind)
            {
                reg_combo->addItem(reg.get_name().c_str());
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
                reg_combo->addItem(reg.get_name().c_str());
                combo_reg_idx.push_back(i);
            }

        }
        register_selected = true;
    }
}


