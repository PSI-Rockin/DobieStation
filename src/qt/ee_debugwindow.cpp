#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QWidget>

#include <QtGui/QFontDatabase>
#include <QMenu>
#include <QTableWidget>
#include "../core/ee/emotion.hpp"
#include "../core/int128.hpp"
#include "ee_debugwindow.hpp"
#include "../core/ee/emotiondisasm.hpp"
#include "../core/iop/iop.hpp"
#include "breakpoint_window.hpp"
#include "and_breakpoint_window.hpp"
#include "emuthread.hpp"

#define EE_DEBUGGER_DISASSEMBLY_SIZE (256)
#define EE_DEBUGGER_MAX_BREAKPOINTS 256
#define EE_DEBUGGER_MEMORY_SIZE (256)
#define EE_DEBUGGER_STACK_SIZE (256)

static const char* breakpoint_names[] = {"And", "Memory", "Register", "PC", "Step"};



void EEDebugWindow::build_ui()
{
    this->resize(1600, 900);
    main_grid_layout = new QGridLayout(this);

    registers_group = new QGroupBox(this);
    registers_grid_layout = new QGridLayout(registers_group);
    register_table = new QTableWidget(registers_group);
    register_table->setContextMenuPolicy(Qt::CustomContextMenu);
    register_table->setWordWrap(false);
    registers_grid_layout->addWidget(register_table, 0, 0, 1, 1);
    main_grid_layout->addWidget(registers_group, 0, 2, 1, 1);

    stack_layout = new QGridLayout();
    stack_group = new QGroupBox(this);
    stack_grid_layout = new QGridLayout(stack_group);
    stack_table = new QTableWidget(stack_group);
    stack_table->setWordWrap(false);
    stack_table->setContextMenuPolicy(Qt::CustomContextMenu);
    stack_grid_layout->addWidget(stack_table, 0, 1, 1, 1);
    stack_layout->addWidget(stack_group, 0, 0, 1, 1);
    main_grid_layout->addLayout(stack_layout, 1, 0, 1, 1);


    control_breakpoint_layout = new QGridLayout();
    control_group = new QGroupBox(this);
    control_grid_layout = new QGridLayout(control_group);
    add_breakpoint_combo = new QComboBox(control_group);
    control_grid_layout->addWidget(add_breakpoint_combo, 0, 4, 1, 1);
    step_button = new QPushButton(control_group);
    control_grid_layout->addWidget(step_button, 0, 1, 1, 1);
    break_continue_button = new QPushButton(control_group);
    control_grid_layout->addWidget(break_continue_button, 0, 0, 1, 1);
    next_button = new QPushButton(control_group);
    control_grid_layout->addWidget(next_button, 1, 0, 1, 1);
    finish_button = new QPushButton(control_group);
    control_grid_layout->addWidget(finish_button, 1, 1, 1, 1);
    cpu_combo = new QComboBox(control_group);
    control_grid_layout->addWidget(cpu_combo, 1, 4, 1, 1);
    control_breakpoint_layout->addWidget(control_group, 0, 0, 1, 1);

    breakpoint_group = new QGroupBox(this);
    breakpoint_grid_layout = new QGridLayout(breakpoint_group);
    breakpoint_table = new QTableWidget(breakpoint_group);
    breakpoint_table->setWordWrap(false);
    breakpoint_table->setContextMenuPolicy(Qt::CustomContextMenu);
    breakpoint_grid_layout->addWidget(breakpoint_table, 0, 0, 1, 1);
    control_breakpoint_layout->addWidget(breakpoint_group, 1, 0, 1, 1);
    main_grid_layout->addLayout(control_breakpoint_layout, 1, 2, 1, 1);

    memory_group = new QGroupBox(this);
    memory_grid_layout = new QGridLayout(memory_group);
    memory_seek_ledit = new QLineEdit(memory_group);
    memory_seek_ledit->setMinimumSize(QSize(100, 0));
    memory_grid_layout->addWidget(memory_seek_ledit, 3, 1, 1, 1);
    memory_up = new QPushButton(memory_group);
    memory_grid_layout->addWidget(memory_up, 0, 2, 1, 1);
    memory_seek_to = new QLabel(memory_group);
    memory_grid_layout->addWidget(memory_seek_to, 3, 0, 1, 1);
    memory_spacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);
    memory_grid_layout->addItem(memory_spacer, 1, 2, 1, 1);
    memory_down = new QPushButton(memory_group);
    memory_grid_layout->addWidget(memory_down, 2, 2, 1, 1);
    memory_table = new QTableWidget(memory_group);
    memory_table->setWordWrap(false);
    memory_table->setContextMenuPolicy(Qt::CustomContextMenu);
    memory_grid_layout->addWidget(memory_table, 0, 0, 3, 2);
    memory_go_button = new QPushButton(memory_group);
    memory_go_button->setMinimumSize(QSize(1, 1));
    memory_grid_layout->addWidget(memory_go_button, 3, 2, 1, 1);
    main_grid_layout->addWidget(memory_group, 0, 0, 1, 1);

    disassembly_group = new QGroupBox(this);
    disassembly_grid_layout = new QGridLayout(disassembly_group);
    disassembly_seek_ledit = new QLineEdit(disassembly_group);
    disassembly_seek_ledit->setMinimumSize(QSize(100, 0));
    disassembly_grid_layout->addWidget(disassembly_seek_ledit, 4, 1, 1, 1);
    disassembly_down_button = new QPushButton(disassembly_group);
    disassembly_grid_layout->addWidget(disassembly_down_button, 2, 3, 1, 1);
    seek_to_label = new QLabel(disassembly_group);
    disassembly_grid_layout->addWidget(seek_to_label, 4, 0, 1, 1);
    disassembly_go_button = new QPushButton(disassembly_group);
    disassembly_go_button->setMinimumSize(QSize(70, 20));
    disassembly_grid_layout->addWidget(disassembly_go_button, 4, 2, 1, 1);
    disassembly_up_button = new QPushButton(disassembly_group);
    disassembly_grid_layout->addWidget(disassembly_up_button, 0, 3, 1, 1);
    disassembly_spacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);
    disassembly_grid_layout->addItem(disassembly_spacer, 1, 3, 1, 1);
    disassembly_seek_to_pc_button = new QPushButton(disassembly_group);
    disassembly_grid_layout->addWidget(disassembly_seek_to_pc_button, 4, 3, 1, 1);
    disassembly_table = new QTableWidget(disassembly_group);
    disassembly_table->setWordWrap(false);
    disassembly_table->setContextMenuPolicy(Qt::CustomContextMenu);
    disassembly_grid_layout->addWidget(disassembly_table, 0, 0, 3, 3);
    main_grid_layout->addWidget(disassembly_group, 0, 1, 2, 1);

    this->setWindowTitle(QApplication::translate("EEDebugWindow", "Form", nullptr));
    registers_group->setTitle(QApplication::translate("EEDebugWindow", "Registers", nullptr));
    stack_group->setTitle(QApplication::translate("EEDebugWindow", "Stack", nullptr));
    control_group->setTitle(QApplication::translate("EEDebugWindow", "Control", nullptr));
    step_button->setText(QApplication::translate("EEDebugWindow", "Step", nullptr));
    break_continue_button->setText(QApplication::translate("EEDebugWindow", "Continue/Break", nullptr));
    next_button->setText(QApplication::translate("EEDebugWindow", "Next", nullptr));
    finish_button->setText(QApplication::translate("EEDebugWindow", "Finish", nullptr));
    breakpoint_group->setTitle(QApplication::translate("EEDebugWindow", "Breakpoints", nullptr));
    memory_group->setTitle(QApplication::translate("EEDebugWindow", "Memory", nullptr));
    memory_up->setText(QApplication::translate("EEDebugWindow", "Up 2k", nullptr));
    memory_seek_to->setText(QApplication::translate("EEDebugWindow", "Seek to:", nullptr));
    memory_down->setText(QApplication::translate("EEDebugWindow", "Down 2k", nullptr));
    memory_go_button->setText(QApplication::translate("EEDebugWindow", "Go", nullptr));
    disassembly_group->setTitle(QApplication::translate("EEDebugWindow", "Disassembly", nullptr));
    disassembly_down_button->setText(QApplication::translate("EEDebugWindow", "Down 2k", nullptr));
    seek_to_label->setText(QApplication::translate("EEDebugWindow", "Seek to:", nullptr));
    disassembly_go_button->setText(QApplication::translate("EEDebugWindow", "Go", nullptr));
    disassembly_up_button->setText(QApplication::translate("EEDebugWindow", "Up 2k", nullptr));
    disassembly_seek_to_pc_button->setText(QApplication::translate("EEDebugWindow", "Seek to PC", nullptr));

    connect(disassembly_up_button, SIGNAL(clicked()), this, SLOT(on_disassembly_up_button_clicked()));
    connect(disassembly_down_button, SIGNAL(clicked()), this, SLOT(on_disassembly_down_button_clicked()));
    connect(disassembly_seek_to_pc_button, SIGNAL(clicked()), this, SLOT(on_disassembly_seek_to_pc_button_clicked()));
    connect(disassembly_go_button, SIGNAL(clicked()), this, SLOT(on_disassembly_go_button_clicked()));
    connect(disassembly_table, SIGNAL(cellDoubleClicked(int, int)), this, SLOT(on_disassembly_table_cellDoubleClicked(int, int)));

    connect(memory_up, SIGNAL(clicked()), this, SLOT(on_memory_up_clicked()));
    connect(memory_down, SIGNAL(clicked()), this, SLOT(on_memory_down_clicked()));
    connect(memory_go_button, SIGNAL(clicked()), this, SLOT(on_memory_go_button_clicked()));
    connect(memory_seek_ledit, SIGNAL(returnPressed()), this, SLOT(on_memory_seek_ledit_returnPressed()));

    connect(break_continue_button, SIGNAL(clicked()), this, SLOT(on_break_continue_button_clicked()));
    connect(step_button, SIGNAL(clicked()), this, SLOT(on_step_button_clicked()));
    connect(next_button, SIGNAL(clicked()), this, SLOT(on_next_button_clicked()));
    connect(finish_button, SIGNAL(clicked()), this, SLOT(on_finish_button_clicked()));
    connect(add_breakpoint_combo, SIGNAL(activated(int )), this, SLOT(on_add_breakpoint_combo_activated(int)));
    connect(cpu_combo, SIGNAL(activated(int)), this, SLOT(on_cpu_combo_activated(int)));
    connect(breakpoint_table, SIGNAL(cellDoubleClicked(int, int)), this, SLOT(on_breakpoint_table_cellDoubleClicked(int, int)));
}
// set formatting for qtable (monospaced font, no borders)
static void setup_qtable(QTableWidget* table, uint32_t size)
{
    table->verticalHeader()->hide();
    table->horizontalHeader()->hide();
    table->setShowGrid(false);

    table->setEditTriggers(QAbstractItemView::NoEditTriggers); // prevent editing
    table->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont)); // does this work on mac/windows?
    table->setColumnCount(1);
    table->setRowCount(size);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); // ??
    for (uint32_t i = 0; i < size; i++)
    {
        table->setItem(i,0,new QTableWidgetItem("uninitialized"));
        table->setRowHeight(i, 20);
    }
}

// 128-bit register value to hex
static QString reg_to_string(uint128_t value)
{
  return QString("%1 %2 %3 %4")
      .arg(value._u32[3], 8, 16, QChar('0'))
      .arg(value._u32[2], 8, 16, QChar('0'))
      .arg(value._u32[1], 8, 16, QChar('0'))
      .arg(value._u32[0], 8, 16, QChar('0'));
}

static QString reg_to_string(uint64_t value)
{
    char cstr[128];
    sprintf(cstr, "%08X %08X", (uint32_t)(value >> 32), (uint32_t)(value & 0xffffffff));
    return QString(cstr);
}

static QString reg_to_string(uint32_t value)
{
    char cstr[128];
    sprintf(cstr, "%08X", value);
    return QString(cstr);
}

static QString reg_to_string(float value)
{
    char cstr[128];
    sprintf(cstr, "%g", value);
    return QString(cstr);
}

// word as hex, ascii, floating point
static QString memory_data_to_string(uint32_t data)
{
    char* charp = (char*)(&data);

    char ascii[5];

    for (int j = 0; j < 4; j++)
    {
        if(charp[j] >= ' ' && charp[j] <= '~')
            ascii[j] = charp[j];
        else
            ascii[j] = '.';
    }
    ascii[4] = '\0';
    char fp[64];
    sprintf(fp, "%g", *(float*)(&data));
    return QString("%1 %2 %3").arg(reg_to_string(data)).arg(ascii).arg(fp);
}

static uint32_t hex_address_to_u32(QString str)
{
    char* charp = str.toUtf8().data();
    return strtoul(charp, nullptr, 16);
}


void EEDebugWindow::set_breakpoint_list(EEBreakpointList *list)
{
    ee_breakpoint_list = list;
}

void EEDebugWindow::set_breakpoint_list(IOPBreakpointList *list)
{
    iop_breakpoint_list = list;
}

void EEDebugWindow::update_break_continue_button()
{
    if(emulation_running())
        break_continue_button->setText("Break");
    else
        break_continue_button->setText("Continue");
}

void EEDebugWindow::setup_ui_formatting()
{
    setup_qtable(disassembly_table, EE_DEBUGGER_DISASSEMBLY_SIZE);
    setup_qtable(breakpoint_table, EE_DEBUGGER_MAX_BREAKPOINTS);
    setup_qtable(memory_table, EE_DEBUGGER_MEMORY_SIZE);
    setup_qtable(register_table, ee_registers.size() + 1); // +1 for the PC
    setup_qtable(stack_table, EE_DEBUGGER_STACK_SIZE);

    add_breakpoint_combo->addItem("Add Breakpoint...");
    for(auto name : breakpoint_names)
    {
        add_breakpoint_combo->addItem(QString(name));
    }

    cpu_combo->addItem("EE");
    cpu_combo->addItem("IOP");
}

void EEDebugWindow::update_ui()
{
    update_break_continue_button();
    update_registers();
    update_disassembly();
    update_memory();
    update_stack();
    update_breakpoints();
}

void EEDebugWindow::update_registers()
{
    uint32_t r = 0;
    auto set_next = [&r, this](QString q){register_table->item(r++,0)->setText(q);};

    if(current_cpu == DebuggerCpu::EE)
    {
        // program counter
        set_next(QString("pc %1").arg(reg_to_string(ee->get_PC())));

      for(auto& reg : ee_registers)
        {
            if(ee_breakpoint_list->get_breakpoints_at_register(reg).empty())
                register_table->item(r,0)->setBackgroundColor(Qt::white);
            else
                register_table->item(r,0)->setBackgroundColor(Qt::green);

            switch(reg.kind)
            {
                case EERegisterKind::GPR:
                    set_next(QString("%1 %2").arg(reg.get_name().c_str()).arg(reg_to_string(reg.get<uint128_t>(ee))));
                    break;
                case EERegisterKind::HI:
                case EERegisterKind::LO:
                case EERegisterKind::HI1:
                case EERegisterKind::LO1:
                    set_next(QString("%1 %2").arg(reg.get_name().c_str()).arg(reg_to_string(reg.get<uint64_t>(ee))));
                    break;
                case EERegisterKind::CP1:
                    set_next(QString("%1 %2").arg(reg.get_name().c_str()).arg(reg_to_string(reg.get<float>(ee))));
                break;
                case EERegisterKind::CP0:
                {
                    if(reg.id == 25)
                    {
                        QString cop0_str;
                        cop0_str = reg_to_string(ee->get_cop0()->PCCR) + "/";
                        cop0_str += reg_to_string(ee->get_cop0()->PCR0) + "/";
                        cop0_str += reg_to_string(ee->get_cop0()->PCR1);
                        set_next(QString("%1 %2").arg(reg.get_name().c_str()).arg(cop0_str));
                    }
                    else
                    {
                        set_next(QString("%1 %2").arg(reg.get_name().c_str()).arg(reg_to_string(reg.get<uint32_t>(ee))));
                    }
                    break;
                }
                default:
                    Errors::die("unknown register kind");
            }
        }
    }
    else if(current_cpu == DebuggerCpu::IOP)
    {
        // program counter
        set_next(QString("pc %1").arg(reg_to_string(iop->get_PC())));

        for(auto& reg : iop_registers)
        {
            if(iop_breakpoint_list->get_breakpoints_at_register(reg).empty())
                register_table->item(r,0)->setBackgroundColor(Qt::white);
            else
                register_table->item(r,0)->setBackgroundColor(Qt::green);

            set_next(QString("%1 %2").arg(reg.get_name().c_str()).arg(reg_to_string(reg.get<uint32_t>(iop))));
        }

        while(r < ee_registers.size())
            set_next("");
    }

}

void EEDebugWindow::update_disassembly()
{
    uint32_t r = 0;
    auto set_next = [&r, this](QString q){disassembly_table->item(r++,0)->setText(q);};
    uint32_t start_addr = get_current_cpu_disassembly_location() - EE_DEBUGGER_DISASSEMBLY_SIZE * 2;
    uint32_t end_addr = start_addr + EE_DEBUGGER_DISASSEMBLY_SIZE * 4;
    for (int i = 0; i < EE_DEBUGGER_DISASSEMBLY_SIZE * 4; i += 4)
    {
        uint32_t addr = start_addr + i;
        QString addr_str = QString("[%1] ").arg(reg_to_string(addr));
        QString table_str;
        try
        {

            uint32_t instr = 0;
            if(current_cpu == DebuggerCpu::EE)
                instr = ee->read32(addr);
            else if(current_cpu == DebuggerCpu::IOP)
                instr = iop->read32(addr);

            QString instr_str = QString("%1 ").arg(reg_to_string(instr));
            QString dis_str = QString::fromStdString(EmotionDisasm::disasm_instr(instr, addr));

            if (addr == get_current_cpu_recent_pc())
                addr_str = "->" + addr_str;
            else
                addr_str = "  " + addr_str;


            table_str = (addr_str + instr_str + dis_str);

        } catch(std::exception& e) {
            table_str = (addr_str + " bad memory!");
        }
        disassembly_table->item(r,0)->setBackgroundColor(Qt::white);
        set_next(table_str);
    }

    auto addr_to_highlight = get_current_cpu_breakpoints()->get_instruction_address_list();

    for(auto addr : addr_to_highlight)
    {
        if(addr >= start_addr && addr < end_addr)
        {
             uint32_t row = (addr - start_addr)/4;
             disassembly_table->item(row,0)->setBackgroundColor(Qt::green);
        }
    }
}


void EEDebugWindow::update_memory()
{
    uint32_t r = 0;
    auto set_next = [&r, this](QString q){memory_table->item(r++,0)->setText(q);};

    uint32_t start_addr = get_current_cpu_memory_location() - EE_DEBUGGER_MEMORY_SIZE * 2;
    uint32_t end_addr = start_addr + EE_DEBUGGER_MEMORY_SIZE*4;
    for (int i = 0; i < EE_DEBUGGER_MEMORY_SIZE * 4; i += 4)
    {
        uint32_t addr = start_addr + i;
        QString addr_str = QString("[%1] ").arg(reg_to_string(addr));
        QString table_str;

        try
        {
            uint32_t data = 0;

            if(current_cpu == DebuggerCpu::EE)
                data = ee->read32(addr);
            else if(current_cpu == DebuggerCpu::IOP)
                data = iop->read32(addr);

            table_str = (addr_str + memory_data_to_string(data));
        }
        catch (std::exception& e)
        {
            table_str = (addr_str + "bad memory!");
        }
        memory_table->item(r,0)->setBackgroundColor(Qt::white);
        set_next(table_str);
    }
    //memory_table->scrollToItem(memory_table->item(EE_DEBUGGER_MEMORY_SIZE/2,0), QAbstractItemView::PositionAtCenter);

    auto addr_to_highlight = get_current_cpu_breakpoints()->get_memory_address_list();
    for(auto addr : addr_to_highlight)
    {
        if(addr >= start_addr && addr < end_addr)
        {
            uint32_t row = (addr - start_addr)/4;
            memory_table->item(row,0)->setBackgroundColor(Qt::green);
        }
    }
}

void EEDebugWindow::update_stack()
{
    uint32_t r = 0;
    auto set_next = [&r, this](QString q){stack_table->item(r++,0)->setText(q);};

    uint32_t sp = 0;
    if(current_cpu == DebuggerCpu::EE)
        sp = ee->get_gpr<uint32_t>(29);
    else if(current_cpu == DebuggerCpu::IOP)
        sp = iop->get_gpr(29);

    for (int i = 4; i <= EE_DEBUGGER_STACK_SIZE * 4; i += 4)
    {
        uint32_t offset = EE_DEBUGGER_STACK_SIZE*4 - i;
        uint32_t addr = sp + offset;
        QString addr_str = QString("[sp+%1] ").arg(reg_to_string(offset));
        QString table_str;
        try
        {
            uint32_t data = 0;

            if(current_cpu == DebuggerCpu::EE)
                data = ee->read32(addr);
            else if(current_cpu == DebuggerCpu::IOP)
                data = iop->read32(addr);
            table_str = (addr_str + memory_data_to_string(data));
        }
        catch (std::exception& e)
        {
            table_str = (addr_str + "bad memory!");
        }
        set_next(table_str);
    }
}

void EEDebugWindow::update_breakpoints()
{
    uint32_t r = 0;
    auto set_next = [&r, this](QString q){breakpoint_table->item(r++,0)->setText(q);};

    BreakpointList* breakpoints = get_current_cpu_breakpoints();

    for (auto& bp : breakpoints->get_breakpoints())
    {
        if(breakpoints->get_breakpoint_status()[r])
            breakpoint_table->item(r,0)->setBackgroundColor(Qt::lightGray);
        else
            breakpoint_table->item(r,0)->setBackgroundColor(Qt::white);
        set_next(QString("%1: %2").arg(QString::number(r)).arg(QString(bp->get_description().c_str())));
    }

    for(uint32_t i = breakpoints->get_breakpoints().size(); i < EE_DEBUGGER_MAX_BREAKPOINTS; i++)
    {
        breakpoint_table->item(i,0)->setBackgroundColor(Qt::white);
        set_next(QString(""));
    }

    breakpoint_table->scrollToItem(breakpoint_table->item(0,0), QAbstractItemView::PositionAtTop);
}


EEDebugWindow::EEDebugWindow(EmuThread* emu_thread, QWidget *parent) :
        QWidget(parent),
        emu_thread(emu_thread)
{
    build_ui();
    setWindowTitle("EE Debugger");
    connect(this, SIGNAL(breakpoint_signal(EmotionEngine*)), this, SLOT(breakpoint_hit(EmotionEngine*)));
    connect(this, SIGNAL(breakpoint_signal(IOP*)), this, SLOT(breakpoint_hit(IOP*)));
    connect(this, SIGNAL(pause_signal()), this, SLOT(pause_hit()));
    memory_table->setContextMenuPolicy(Qt::CustomContextMenu);
    disassembly_table->setContextMenuPolicy(Qt::CustomContextMenu);
    register_table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(memory_table, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(memory_context_menu(const QPoint &)));
    connect(disassembly_table, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(instruction_context_menu(const QPoint &)));
    connect(register_table, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(register_context_menu(const QPoint &)));
    ee_running = true;
    iop_running = true;

    ee_disassembly_location = 0x1000; // no clue, better not to put it at zero i think
    iop_disassembly_location = 0x1000;
    ee_most_recent_pc = ee_disassembly_location;
    iop_most_recent_pc = iop_disassembly_location;
    ee_memory_location = 0x1000;
    iop_memory_location = 0x1000;

    // add registers
    for(uint32_t i = 1; i < 32; i++)
    {
        ee_registers.emplace_back(EERegisterKind::GPR, i);
    }

    ee_registers.emplace_back(EERegisterKind::HI);
    ee_registers.emplace_back(EERegisterKind::LO);
    ee_registers.emplace_back(EERegisterKind::HI1);
    ee_registers.emplace_back(EERegisterKind::LO1);

    for(uint32_t i = 0; i < 32; i++)
    {
        EERegisterDefinition reg(EERegisterKind::CP0, i);
        if(reg.get_name() != "---")
            ee_registers.push_back(reg);
    }

    for(uint32_t i = 1; i < 32; i++)
    {
        ee_registers.emplace_back(EERegisterKind::CP1, i);
    }

    for(uint32_t i = 1; i < 32; i++)
    {
        iop_registers.emplace_back(IOPRegisterKind::GPR, i);
    }

    iop_registers.emplace_back(IOPRegisterKind::HI);
    iop_registers.emplace_back(IOPRegisterKind::LO);

    update_break_continue_button();
    setup_ui_formatting();
}

void EEDebugWindow::memory_context_menu(const QPoint &pos)
{
    if(emulation_running()) return;
    int32_t row = memory_table->currentRow();
    uint32_t addr = get_current_cpu_memory_location() - EE_DEBUGGER_MEMORY_SIZE * 2 + row * 4;


    auto bps = get_current_cpu_breakpoints()->get_breakpoints_at_memory_addr(addr);

    QMenu menu;
    menu.addAction(tr("Add new memory breakpoint"), this, [=]{
        breakpoint_window bpw;
        DebuggerBreakpoint* new_bp = bpw.make_memory_breakpoint(addr, current_cpu == DebuggerCpu::IOP);
        if(new_bp)
            add_breakpoint(new_bp);
    });

    menu.addAction(tr("Add new memory AND breakpoint"), this, [&]{
       breakpoint_window mem_bpw;
       DebuggerBreakpoint* mem_bp = mem_bpw.make_memory_breakpoint(addr, current_cpu == DebuggerCpu::IOP);
       if(!mem_bp) return;
       AndBreakpointWindow and_bpw;
       if(current_cpu == DebuggerCpu::EE)
       {
           DebuggerBreakpoint* and_bp = and_bpw.make_and_breakpoint(&ee_registers, mem_bp);
           if(and_bp)
               add_breakpoint(and_bp);
       }
       else if (current_cpu == DebuggerCpu::IOP)
       {
           DebuggerBreakpoint* and_bp = and_bpw.make_and_breakpoint(&iop_registers, mem_bp);
           if(and_bp)
               add_breakpoint(and_bp);
       }

    });

    if(!bps.empty())
    {
        menu.addSeparator();
        for(uint64_t bp : bps)
        {
            QString menu_item_name = QString("Delete %1").arg(get_current_cpu_breakpoints()->get_breakpoints()[bp]->get_description().c_str());
            menu.addAction(menu_item_name, this, [=]{ delete_breakpoint_by_index(bp);});
        }
    }

    menu.exec(QCursor::pos());
}

void EEDebugWindow::instruction_context_menu(const QPoint &pos)
{
    if(emulation_running()) return;
    int32_t row = disassembly_table->currentRow();
    uint32_t addr = get_current_cpu_disassembly_location() - EE_DEBUGGER_DISASSEMBLY_SIZE * 2 + row * 4;
    auto bps = get_current_cpu_breakpoints()->get_breakpoints_at_instruction_addr(addr);

    QMenu menu;
    menu.addAction(tr("Add Breakpoint (persistent)"), this, [=]{add_breakpoint(new EmotionBreakpoints::PC(addr, false));});
    menu.addAction(tr("Add Breakpoint (one-time)"), this, [=]{add_breakpoint(new EmotionBreakpoints::PC(addr, true));});

    menu.addAction(tr("Add new PC AND breakpoint..."), this, [&]{
        DebuggerBreakpoint* mem_bp = new EmotionBreakpoints::PC(addr, false);
        AndBreakpointWindow and_bpw;

        if(current_cpu == DebuggerCpu::EE)
        {
            DebuggerBreakpoint* and_bp = and_bpw.make_and_breakpoint(&ee_registers, mem_bp);
            if(and_bp)
                add_breakpoint(and_bp);
        }
        else if (current_cpu == DebuggerCpu::IOP)
        {
            DebuggerBreakpoint* and_bp = and_bpw.make_and_breakpoint(&iop_registers, mem_bp);
            if(and_bp)
                add_breakpoint(and_bp);
        }

    });


    if(!bps.empty())
    {
        menu.addSeparator();
        for(uint64_t bp : bps)
        {
            QString menu_item_name = QString("Delete %1").arg(get_current_cpu_breakpoints()->get_breakpoints()[bp]->get_description().c_str());
            menu.addAction(menu_item_name, this, [=]{delete_breakpoint_by_index(bp);});
        }
    }
    menu.exec(QCursor::pos());
}

void EEDebugWindow::register_context_menu(const QPoint &qpos)
{
    if(emulation_running()) return;
    int32_t row = register_table->currentRow();
    QMenu menu;
    if(current_cpu == DebuggerCpu::EE)
    {
        auto reg_def = ee_registers[row - 1]; // hack for pc
        auto bps = ee_breakpoint_list->get_breakpoints_at_register(reg_def);


        menu.addAction(tr("Add Breakpoint..."), this, [&]{
            breakpoint_window bpw;
            DebuggerBreakpoint* new_bp = bpw.make_register_breakpoint(reg_def);
            if(new_bp)
                add_breakpoint(new_bp);
        });

        menu.addAction(tr("Add new Register AND breakpoint..."), this, [&]{
            breakpoint_window bpw;
            DebuggerBreakpoint* reg_bp = bpw.make_register_breakpoint(reg_def);
            if(!reg_bp) return;
            AndBreakpointWindow and_bpw;
            DebuggerBreakpoint* and_bp = and_bpw.make_and_breakpoint(&ee_registers, reg_bp);
            if(and_bp)
                add_breakpoint(and_bp);
        });

        if(!bps.empty())
        {
            menu.addSeparator();
            for(uint64_t bp : bps)
            {
                QString menu_item_name = QString("Delete %1").arg(ee_breakpoint_list->get_breakpoints()[bp]->get_description().c_str());
                menu.addAction(menu_item_name, this, [=]{delete_breakpoint_by_index(bp);});
            }
        }
    }
    else
    {
        auto reg_def = iop_registers[row - 1]; // hack for pc
        auto bps = iop_breakpoint_list->get_breakpoints_at_register(reg_def);

        menu.addAction(tr("Add Breakpoint..."), this, [&]{
            breakpoint_window bpw;
            DebuggerBreakpoint* new_bp = bpw.make_register_breakpoint(reg_def);
            if(new_bp)
                add_breakpoint(new_bp);
        });

        menu.addAction(tr("Add new Register AND breakpoint..."), this, [&]{
            breakpoint_window bpw;
            DebuggerBreakpoint* reg_bp = bpw.make_register_breakpoint(reg_def);
            if(!reg_bp) return;
            AndBreakpointWindow and_bpw;
            DebuggerBreakpoint* and_bp = and_bpw.make_and_breakpoint(&iop_registers, reg_bp);
            if(and_bp)
                add_breakpoint(and_bp);
        });

        if(!bps.empty())
        {
            menu.addSeparator();
            for(uint64_t bp : bps)
            {
                QString menu_item_name = QString("Delete %1").arg(iop_breakpoint_list->get_breakpoints()[bp]->get_description().c_str());
                menu.addAction(menu_item_name, this, [=]{delete_breakpoint_by_index(bp);});
            }
        }
    }

    menu.exec(QCursor::pos());
}

EEDebugWindow::~EEDebugWindow()
{

}

void EEDebugWindow::on_disassembly_up_button_clicked()
{
    set_current_cpu_disassembly_location(get_current_cpu_disassembly_location() - 2048);
    update_disassembly();
}

void EEDebugWindow::on_disassembly_down_button_clicked()
{
    set_current_cpu_disassembly_location(get_current_cpu_disassembly_location() + 2048);
    update_disassembly();
}

void EEDebugWindow::on_disassembly_seek_to_pc_button_clicked()
{
    set_current_cpu_disassembly_location(get_current_cpu_recent_pc());
    update_disassembly();
}

void EEDebugWindow::on_disassembly_go_button_clicked()
{
    QString dest = disassembly_seek_ledit->text();
    set_current_cpu_disassembly_location(hex_address_to_u32(dest));
    update_disassembly();
    disassembly_table->scrollToItem(disassembly_table->item(EE_DEBUGGER_DISASSEMBLY_SIZE/2,0), QAbstractItemView::PositionAtCenter);
}

void EEDebugWindow::on_memory_up_clicked()
{
    set_current_cpu_memory_location(get_current_cpu_memory_location() - 2048);
    update_memory();
}

void EEDebugWindow::on_memory_down_clicked()
{
    set_current_cpu_memory_location(get_current_cpu_memory_location() + 2048);
    update_memory();
}

void EEDebugWindow::on_memory_go_button_clicked()
{
    QString dest = memory_seek_ledit->text();
    set_current_cpu_memory_location(hex_address_to_u32(dest));
    update_memory();
    memory_table->scrollToItem(memory_table->item(EE_DEBUGGER_MEMORY_SIZE/2,0), QAbstractItemView::PositionAtCenter);
}


void EEDebugWindow::on_break_continue_button_clicked()
{
    printf("[Debugger] Waiting for emulator thread to pause...\n");
    if(emulation_running())
    {
        waiting_for_pause = true;
        emu_thread->pause(DEBUGGER);
    }

        //add_breakpoint(new EmotionBreakpoints::Step());
    else
        resume_emulator();
    //printf("break handler returning!\n");
}

void EEDebugWindow::pause_on_show()
{
    if(emulation_running())
    {
        waiting_for_pause = true;
        emu_thread->pause(DEBUGGER);
    }
}

void EEDebugWindow::on_step_button_clicked()
{
    if(!emulation_running())
    {
        add_breakpoint(new EmotionBreakpoints::Step());
        resume_emulator();
    }
}



void EEDebugWindow::on_next_button_clicked()
{
    if(!emulation_running())
    {
        // check the previous instruction:
        uint32_t last_instruction = 0;
        if(current_cpu == DebuggerCpu::EE)
            last_instruction = ee->read32(ee_most_recent_pc - 4);
        else
            last_instruction = iop->read32(iop_most_recent_pc - 4);

        if(EmotionDisasm::is_function_call(last_instruction))
        {
            printf("is function call, break on PC = 0x%x\n", get_current_cpu_recent_pc() + 4);
            add_breakpoint(new EmotionBreakpoints::PC(get_current_cpu_recent_pc() + 4, true));
            resume_emulator();
        }
        else
        {
            add_breakpoint(new EmotionBreakpoints::Step());
            resume_emulator();
        }
    }
}

/* currently this is not 100% reliable - it only works with code generated by gcc or GOAL compiler
 * it will not work correctly with assembly functions that have multiple returns
 * or functions that do not return with jr ra
 * or weird kernel code
 * however, it is still useful for debugging!
 */
void EEDebugWindow::on_finish_button_clicked()
{
    if(!emulation_running())
    {
        // find jr ra
        for(uint32_t addr = get_current_cpu_recent_pc(); addr < get_current_cpu_recent_pc() + 0x4000; addr+=4)
        {
            uint32_t instr = 0;
            if(current_cpu == DebuggerCpu::EE)
                instr = ee->read32(addr);
            else
                instr = iop->read32(addr);

            if(EmotionDisasm::is_function_return(instr))
            {
                add_breakpoint(new EmotionBreakpoints::Finish(addr));
                resume_emulator();
                return;
            }
        }

        printf("[ERROR] failed to find jr ra - not resuming!\n");
    }
}

void EEDebugWindow::on_add_breakpoint_combo_activated(int index)
{
    if(!emulation_running() && index > 0) // don't select the "add breakpoint..." thing
    {
        DebuggerBreakpoint* bp = nullptr;

        if(current_cpu == DebuggerCpu::EE)
        {
            bp = create_breakpoint_prompt((BreakpointType)(index - 1), &ee_registers);
        }
        else if (current_cpu == DebuggerCpu::IOP)
        {
            bp = create_breakpoint_prompt((BreakpointType)(index - 1), &iop_registers);
        }

        if(bp)
        {
            add_breakpoint(bp);
            update_ui();
        }

    }
    add_breakpoint_combo->setCurrentIndex(0); // back to "add breakpoint..."

}

// function called by the emulator when we hit a breakpoint
// the emulator can't know about qt signals/slots, so this
// causes the breakpoint_hit function to be called in the UI thread
void EEDebugWindow::notify_breakpoint_hit(EmotionEngine *ee)
{
    //printf("notification from emulator thread\n");
    emit breakpoint_signal(ee);
    //printf("returning from emulator thread notify\n");
}

void EEDebugWindow::notify_breakpoint_hit(IOP *iop)
{
    emit breakpoint_signal(iop);
}

void EEDebugWindow::notify_pause_hit()
{
    if(waiting_for_pause)
    {
        printf("[Debugger] Emulator thread is paused!\n");
        emit pause_signal();
        waiting_for_pause = false;
    }
}



// called in the ui thread when a breakpoint is hit
// emulator thread is suspended at this point
void EEDebugWindow::breakpoint_hit(EmotionEngine *ee)
{
    //printf("ui thread update starting...\n");
    ee_running = false;
    current_cpu = DebuggerCpu::EE;
    ee_most_recent_pc = ee->get_PC();
    ee_breakpoint_list->clear_old_breakpoints(); // important to run this in UI thread!!

    // todo : is this what we want?
    ee_disassembly_location = ee_most_recent_pc;
    update_ui();
    cpu_combo->setCurrentIndex((uint32_t)DebuggerCpu::EE);
    disassembly_table->scrollToItem(disassembly_table->item(EE_DEBUGGER_DISASSEMBLY_SIZE/2,0), QAbstractItemView::PositionAtCenter);
    //printf("ui thread done updating!\n");
}

void EEDebugWindow::breakpoint_hit(IOP *iop)
{
    setUpdatesEnabled(false);
    iop_running = false;
    current_cpu = DebuggerCpu::IOP;
    iop_most_recent_pc = iop->get_PC();
    iop_breakpoint_list->clear_old_breakpoints(); // important to run this in UI thread!!

    // todo : is this what we want?
    iop_disassembly_location = iop_most_recent_pc;
    update_ui();
    cpu_combo->setCurrentIndex((uint32_t)DebuggerCpu::IOP);
    disassembly_table->scrollToItem(disassembly_table->item(EE_DEBUGGER_DISASSEMBLY_SIZE/2,0), QAbstractItemView::PositionAtCenter);
    setUpdatesEnabled(true);
    repaint();
}

void EEDebugWindow::pause_hit()
{
    setUpdatesEnabled(false);
    ee_most_recent_pc = ee->get_PC();
    ee_breakpoint_list->clear_old_breakpoints(); // important to run this in UI thread!!
    iop_most_recent_pc = iop->get_PC();
    iop_breakpoint_list->clear_old_breakpoints(); // important to run this in UI thread!!
    ee_disassembly_location = ee_most_recent_pc;
    iop_disassembly_location = iop_most_recent_pc;
    update_ui();
    disassembly_table->scrollToItem(disassembly_table->item(EE_DEBUGGER_DISASSEMBLY_SIZE/2,0), QAbstractItemView::PositionAtCenter);
    setUpdatesEnabled(true);
    repaint();
}

void EEDebugWindow::add_breakpoint(DebuggerBreakpoint *breakpoint)
{
    get_current_cpu_breakpoints()->lock_list_mutex();
    get_current_cpu_breakpoints()->get_breakpoints().push_back(breakpoint);
    get_current_cpu_breakpoints()->get_breakpoint_status().push_back(false);
    update_breakpoints();
    update_disassembly();
    update_memory();
    update_registers();
    get_current_cpu_breakpoints()->unlock_list_mutex();
    get_current_cpu_breakpoints()->debug_enable = true; // tells emulator to call do_breakpoints
}

void EEDebugWindow::resume_emulator()
{
    update_breakpoints(); // flush out old breakpoints from UI while emulator runs

    emu_thread->unpause(DEBUGGER);
    if(!ee_running)
    {
        ee_breakpoint_list->send_continue();
        ee_running = true;
    }

    if(!iop_running)
    {
        iop_breakpoint_list->send_continue();
        iop_running = true;
    }


//    get_current_cpu_breakpoints()->send_continue(); // tells emulator to continue running
//    if(DebuggerCpu::EE == current_cpu)
//        ee_running = true;
//    else
//        iop_running = true;

    update_break_continue_button();
}


void EEDebugWindow::set_ee(EmotionEngine *e)
{
    ee = e;
}

void EEDebugWindow::set_iop(IOP* i)
{
    iop = i;
}

bool EEDebugWindow::emulation_running()
{
    return ee_running && iop_running && !emu_thread->is_paused();
}




void EEDebugWindow::on_disassembly_table_cellDoubleClicked(int row, int column)
{
    uint32_t addr = (row*4) + get_current_cpu_disassembly_location() - EE_DEBUGGER_DISASSEMBLY_SIZE * 2;
    if(!emulation_running())
    {
        int32_t breakpoint_to_remove = -1;
        auto& breakpoints = get_current_cpu_breakpoints()->get_breakpoints();
        auto& breakpoint_status = get_current_cpu_breakpoints()->get_breakpoint_status();
        for(uint32_t i = 0; i < breakpoints.size(); i++)
        {
            if(breakpoints[i]->get_instruction_address() == addr)
                breakpoint_to_remove = i;
        }

        if(breakpoint_to_remove == -1)
            add_breakpoint(new EmotionBreakpoints::PC(addr));
        else
            remove_breakpoint(breakpoint_to_remove);
    }
}

void EEDebugWindow::on_breakpoint_table_cellDoubleClicked(int row, int column)
{
    if (!emulation_running())
    {
        auto &list = get_current_cpu_breakpoints()->get_breakpoints();
        if (row < list.size())
            remove_breakpoint(row);
    }
}

void EEDebugWindow::remove_breakpoint(uint64_t id)
{
    auto& list = get_current_cpu_breakpoints()->get_breakpoints();
    auto& status_list = get_current_cpu_breakpoints()->get_breakpoint_status();
    delete list[id];
    list.erase(list.begin() + id);
    status_list.erase(status_list.begin() + id);
    update_breakpoints();
    update_disassembly();
    update_memory();
    update_registers();
}


// prompt for a breakpoint of unknown type
// needs to know what regs are currently available beacuse we might have an and
DebuggerBreakpoint *EEDebugWindow::create_breakpoint_prompt(BreakpointType type, std::vector<EERegisterDefinition>* ee_regs)
{
    switch(type)
    {
        case BreakpointType::AND:
        {
            AndBreakpointWindow bpw;
            return bpw.make_and_breakpoint(ee_regs);
        }
        case BreakpointType::MEMORY:
        {
            breakpoint_window bpw;
            return bpw.make_memory_breakpoint();
        }
        case BreakpointType::REGISTER:
        {
            breakpoint_window bpw;
            return bpw.make_register_breakpoint(ee_regs);
        }
        case BreakpointType::PC:
        {
            breakpoint_window bpw;
            return bpw.make_pc_breakpoint();
        }
        case BreakpointType::STEP:
            return new EmotionBreakpoints::Step();
    }
}

DebuggerBreakpoint *EEDebugWindow::create_breakpoint_prompt(BreakpointType type, std::vector<IOPRegisterDefinition>* iop_regs)
{
    switch(type)
    {
        case BreakpointType::AND:
        {
            AndBreakpointWindow bpw;
            return bpw.make_and_breakpoint(iop_regs);
        }
        case BreakpointType::MEMORY:
        {
            breakpoint_window bpw;
            return bpw.make_memory_breakpoint(true);
        }
        case BreakpointType::REGISTER:
        {
            breakpoint_window bpw;
            return bpw.make_register_breakpoint(iop_regs);
        }
        case BreakpointType::PC:
        {
            breakpoint_window bpw;
            return bpw.make_pc_breakpoint();
        }
        case BreakpointType::STEP:
            return new EmotionBreakpoints::Step();
    }
}

void EEDebugWindow::on_cpu_combo_activated(int index)
{
    if((DebuggerCpu)index == DebuggerCpu::EE)
    {
        current_cpu = DebuggerCpu::EE;
        ee_most_recent_pc = ee->get_PC();
        ee_disassembly_location = ee_most_recent_pc;
        update_ui();
    }
    else
    {
        current_cpu = DebuggerCpu::IOP;
        iop_most_recent_pc = iop->get_PC();
        iop_disassembly_location = iop_most_recent_pc;
        update_ui();
    }

}

void EEDebugWindow::delete_breakpoint_by_index(uint64_t bp)
{
    remove_breakpoint(bp);
}

BreakpointList *EEDebugWindow::get_current_cpu_breakpoints()
{
    BreakpointList* breakpoints;
    if(current_cpu == DebuggerCpu::EE)
        breakpoints = ee_breakpoint_list;
    else
        breakpoints = iop_breakpoint_list;

    return breakpoints;
}

uint32_t EEDebugWindow::get_current_cpu_memory_location()
{
    if(current_cpu == DebuggerCpu::EE)
        return ee_memory_location;
    else if(current_cpu == DebuggerCpu::IOP)
        return iop_memory_location;
    else
        return 0;
}

uint32_t EEDebugWindow::get_current_cpu_disassembly_location()
{
    if(current_cpu == DebuggerCpu::EE)
        return ee_disassembly_location;
    else if(current_cpu == DebuggerCpu::IOP)
        return iop_disassembly_location;
    else
        return 0;
}

uint32_t EEDebugWindow::get_current_cpu_recent_pc()
{
    if(current_cpu == DebuggerCpu::EE)
        return ee_most_recent_pc;
    else if(current_cpu == DebuggerCpu::IOP)
        return iop_most_recent_pc;
    else
        return 0;
}

void EEDebugWindow::set_current_cpu_disassembly_location(uint32_t addr)
{
    if(current_cpu == DebuggerCpu::EE) {
        ee_disassembly_location = addr;
    } else if(current_cpu == DebuggerCpu::IOP) {
        iop_disassembly_location = addr;
    }
}

void EEDebugWindow::set_current_cpu_memory_location(uint32_t addr)
{
    if(current_cpu == DebuggerCpu::EE) {
        ee_memory_location = addr;
    } else if(current_cpu == DebuggerCpu::IOP) {
        iop_memory_location = addr;
    }
}