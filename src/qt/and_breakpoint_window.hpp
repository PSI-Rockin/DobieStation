#ifndef AND_BREAKPOINT_WINDOW_HPP
#define AND_BREAKPOINT_WINDOW_HPP

#include <QDialog>
#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSpacerItem>
#include "../core/ee/emotion_breakpoint.hpp"



class AndBreakpointWindow : public QDialog
{
    Q_OBJECT

public:
    explicit AndBreakpointWindow(QWidget *parent = nullptr);
    ~AndBreakpointWindow();
    DebuggerBreakpoint* make_and_breakpoint(std::vector<EERegisterDefinition>* ee_regs);
    DebuggerBreakpoint* make_and_breakpoint(std::vector<EERegisterDefinition>* ee_regs, DebuggerBreakpoint* first);
    DebuggerBreakpoint* make_and_breakpoint(std::vector<IOPRegisterDefinition>* iop_regs);
    DebuggerBreakpoint* make_and_breakpoint(std::vector<IOPRegisterDefinition>* iop_regs, DebuggerBreakpoint* first);
    DebuggerBreakpoint* make_and_breakpoint(DebuggerBreakpoint* first_bp);
    DebuggerBreakpoint* make_and_breakpoint();

private:
    DebuggerBreakpoint* _first = nullptr, *_second = nullptr;
    std::vector<EERegisterDefinition>* _ee_regs = nullptr;
    std::vector<IOPRegisterDefinition>* _iop_regs = nullptr;

    void build_ui();

    QGridLayout *grid_layout;
    QLabel *second_condition_label;
    QLabel *first_condition_label;
    QComboBox *first_combo;
    QDialogButtonBox *buttonBox;
    QComboBox *second_combo;
    QSpacerItem *verticalSpacer;
    QLabel *first_label;
    QLabel *second_label;

private slots:
    void on_first_combo_activated(int index);
    void on_second_combo_activated(int index);
};

#endif // AND_BREAKPOINT_WINDOW_HPP
