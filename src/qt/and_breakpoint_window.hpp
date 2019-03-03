#ifndef AND_BREAKPOINT_WINDOW_HPP
#define AND_BREAKPOINT_WINDOW_HPP

#include <QDialog>
#include "../core/ee/emotion_breakpoint.hpp"

namespace Ui {
class AndBreakpointWindow;
}

class AndBreakpointWindow : public QDialog
{
    Q_OBJECT

public:
    explicit AndBreakpointWindow(QWidget *parent = 0);
    ~AndBreakpointWindow();
    DebuggerBreakpoint* make_and_breakpoint(std::vector<EERegisterDefinition>* ee_regs);
    DebuggerBreakpoint* make_and_breakpoint(std::vector<EERegisterDefinition>* ee_regs, DebuggerBreakpoint* first);
    DebuggerBreakpoint* make_and_breakpoint(std::vector<IOPRegisterDefinition>* iop_regs);
    DebuggerBreakpoint* make_and_breakpoint(std::vector<IOPRegisterDefinition>* iop_regs, DebuggerBreakpoint* first);
    DebuggerBreakpoint* make_and_breakpoint(DebuggerBreakpoint* first_bp);
    DebuggerBreakpoint* make_and_breakpoint();

private:
    Ui::AndBreakpointWindow *ui;
    DebuggerBreakpoint* _first = nullptr, *_second = nullptr;
    std::vector<EERegisterDefinition>* _ee_regs = nullptr;
    std::vector<IOPRegisterDefinition>* _iop_regs = nullptr;
private slots:
    void on_first_combo_activated(int index);
    void on_second_combo_activated(int index);
};

#endif // AND_BREAKPOINT_WINDOW_HPP
