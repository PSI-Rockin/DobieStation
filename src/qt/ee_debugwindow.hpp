#ifndef EE_DEBUGWINDOW_H
#define EE_DEBUGWINDOW_H

#include <QWidget>
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
#include "../core/ee/emotion_breakpoint.hpp"

enum class BreakpointType
{
    AND,
    MEMORY,
    REGISTER,
    PC,
    STEP
};

class EmuThread;

class EEDebugWindow : public QWidget, public EmotionDebugUI
{

    enum class DebuggerCpu
    {
        EE,
        IOP
    };

    Q_OBJECT

public:
    explicit EEDebugWindow(EmuThread* emu_thread, QWidget *parent = 0);
    ~EEDebugWindow();

    void set_breakpoint_list(EEBreakpointList *list);
    void set_breakpoint_list(IOPBreakpointList *list);
    void notify_breakpoint_hit(EmotionEngine *ee);
    void notify_breakpoint_hit(IOP* iop);
    void notify_pause_hit();
    void update_ui();
    void set_ee(EmotionEngine* e);
    void set_iop(IOP* iop);
    // breakpoint creation ui
    static DebuggerBreakpoint* create_breakpoint_prompt(BreakpointType type, std::vector<EERegisterDefinition>* ee_regs);
    static DebuggerBreakpoint* create_breakpoint_prompt(BreakpointType type, std::vector<IOPRegisterDefinition>* iop_regs);

private slots:
    void on_disassembly_up_button_clicked();
    void on_disassembly_down_button_clicked();
    void on_disassembly_seek_to_pc_button_clicked();
    void on_disassembly_go_button_clicked();
    void on_disassembly_table_cellDoubleClicked(int row, int column);
    void on_memory_up_clicked();
    void on_memory_down_clicked();
    void on_memory_go_button_clicked();
    void on_break_continue_button_clicked();
    void on_step_button_clicked();
    void on_next_button_clicked();
    void on_finish_button_clicked();
    void on_add_breakpoint_combo_activated(int index);
    void on_cpu_combo_activated(int index);
    void on_breakpoint_table_cellDoubleClicked(int row, int column);
    void on_disassembly_seek_ledit_returnPressed()
    {
        on_disassembly_go_button_clicked();
    }

    void on_memory_seek_ledit_returnPressed()
    {
        on_memory_go_button_clicked();
    }

private:
    std::vector<EERegisterDefinition> ee_registers;
    std::vector<IOPRegisterDefinition> iop_registers;
    EmotionEngine* ee = nullptr;
    IOP* iop = nullptr;
    EEBreakpointList *ee_breakpoint_list;
    IOPBreakpointList *iop_breakpoint_list;
    EmuThread* emu_thread;
    uint32_t ee_disassembly_location, iop_disassembly_location;
    uint32_t ee_memory_location, iop_memory_location;
    uint32_t ee_most_recent_pc, iop_most_recent_pc;
    bool ee_running = true;
    bool iop_running = true;
    bool waiting_for_pause = true;

    DebuggerCpu current_cpu = DebuggerCpu::EE;

    bool emulation_running();

    void update_break_continue_button();
    void setup_ui_formatting();
    void update_disassembly();
    void update_memory();
    void update_registers();
    void update_stack();
    void update_breakpoints();
    void add_breakpoint(DebuggerBreakpoint* breakpoint);
    void remove_breakpoint(uint64_t id);
    void resume_emulator();
    BreakpointList* get_current_cpu_breakpoints();
    uint32_t& get_current_cpu_memory_location();
    uint32_t& get_current_cpu_disassembly_location();
    uint32_t& get_current_cpu_recent_pc();

    // ui stuff
    QGridLayout *main_grid_layout;
    QGroupBox *registers_group;
    QGridLayout *registers_grid_layout;
    QTableWidget *register_table;
    QGridLayout *stack_layout;
    QGroupBox *stack_group;
    QGridLayout *stack_grid_layout;
    QTableWidget *stack_table;
    QGridLayout *control_breakpoint_layout;
    QGroupBox *control_group;
    QGridLayout *control_grid_layout;
    QComboBox *add_breakpoint_combo;
    QPushButton *step_button;
    QPushButton *break_continue_button;
    QPushButton *next_button;
    QPushButton *finish_button;
    QComboBox *cpu_combo;
    QGroupBox *breakpoint_group;
    QGridLayout *breakpoint_grid_layout;
    QTableWidget *breakpoint_table;
    QGroupBox *memory_group;
    QGridLayout *memory_grid_layout;
    QLineEdit *memory_seek_ledit;
    QPushButton *memory_up;
    QLabel *memory_seek_to;
    QSpacerItem *memory_spacer;
    QPushButton *memory_down;
    QTableWidget *memory_table;
    QPushButton *memory_go_button;
    QGroupBox *disassembly_group;
    QGridLayout *disassembly_grid_layout;
    QLineEdit *disassembly_seek_ledit;
    QPushButton *disassembly_down_button;
    QLabel *seek_to_label;
    QPushButton *disassembly_go_button;
    QPushButton *disassembly_up_button;
    QSpacerItem *disassembly_spacer;
    QPushButton *disassembly_seek_to_pc_button;
    QTableWidget *disassembly_table;

    void build_ui();


private slots:
    void breakpoint_hit(EmotionEngine *ee);
    void breakpoint_hit(IOP *iop);
    void pause_hit();
    void memory_context_menu(const QPoint &pos);
    void instruction_context_menu(const QPoint &pos);
    void register_context_menu(const QPoint &qpos);
    void delete_breakpoint_by_index(uint64_t bp);

signals:
    void breakpoint_signal(EmotionEngine *ee);
    void breakpoint_signal(IOP *iop);
    void pause_signal();

};

#endif // EE_DEBUGWINDOW_H
