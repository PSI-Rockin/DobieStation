#ifndef EE_DEBUGWINDOW_H
#define EE_DEBUGWINDOW_H

#include <QWidget>
#include "../core/ee/emotion_breakpoint.hpp"

enum class BreakpointType
{
    AND,
    MEMORY,
    REGISTER,
    PC,
    STEP
};

namespace Ui {
class EEDebugWindow;
}

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
    void notify_breakpoint_hit();
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
    Ui::EEDebugWindow *ui;
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
