#ifndef BREAKPOINT_WINDOW_H
#define BREAKPOINT_WINDOW_H

#include <QDialog>
#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QSpacerItem>
#include "../core/int128.hpp"
#include "../core/ee/emotion_breakpoint.hpp"

namespace Ui {
class breakpoint_window;
}

class breakpoint_window : public QDialog
{
    Q_OBJECT

public:
    explicit breakpoint_window(QWidget *parent = nullptr);
    DebuggerBreakpoint* make_memory_breakpoint(bool limit_to_32 = false);
    DebuggerBreakpoint* make_memory_breakpoint(uint32_t addr, bool limit_to_32 = false);
    DebuggerBreakpoint* make_register_breakpoint(EERegisterDefinition& def);
    DebuggerBreakpoint* make_register_breakpoint(std::vector<EERegisterDefinition>* available_regs);
    DebuggerBreakpoint* make_register_breakpoint(IOPRegisterDefinition& def);
    DebuggerBreakpoint* make_register_breakpoint(std::vector<IOPRegisterDefinition>* available_regs);
    DebuggerBreakpoint* make_pc_breakpoint();

    ~breakpoint_window();


private slots:
    void on_reg_type_combo_currentIndexChanged(int index);

private:
    void build_ui();
    void setup_comparison_combo_boxes(bool limit_to_32 = false);
    uint32_t get_address();
    DebuggerBreakpoint* make_register_breakpoint(EERegisterDefinition& def,
            EmotionBreakpoints::MemoryComparisonType type, EmotionBreakpoints::MemoryComparisonKind compare_kind,
            bool clear);

    DebuggerBreakpoint* make_register_breakpoint(IOPRegisterDefinition& def,
                                                 EmotionBreakpoints::MemoryComparisonType type, EmotionBreakpoints::MemoryComparisonKind compare_kind,
                                                 bool clear);

    template<typename T>
    T get_value();

    Ui::breakpoint_window *ui;
    std::vector<EERegisterDefinition>* ee_regs = nullptr;
    std::vector<IOPRegisterDefinition>* iop_regs = nullptr;
    std::vector<uint64_t> combo_reg_idx;
    bool register_selected = false;

    QGridLayout *grid_layout;
    QLabel *reg_label;
    QComboBox *reg_type_combo;
    QLabel *address_label;
    QCheckBox *clear_checkbox;
    QLabel *reg_type_label;
    QSpacerItem *verticalSpacer;
    QDialogButtonBox *ok_cancel_button;
    QLineEdit *address_line;
    QComboBox *comparison_combo;
    QLabel *value_label;
    QLabel *comparison_label;
    QComboBox *data_type_combo;
    QLabel *data_type_label;
    QLineEdit *value_line;
    QComboBox *reg_combo;
};

#endif // BREAKPOINT_WINDOW_H
