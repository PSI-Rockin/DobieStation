#include <QtWidgets/QMessageBox>
#include <QtGui/QFontDatabase>
#include "and_breakpoint_window.hpp"
#include "ee_debugwindow.hpp"


static const char* breakpoint_names[] = {"And", "Memory", "Register", "PC", "Step"};

AndBreakpointWindow::AndBreakpointWindow(QWidget *parent) :
    QDialog(parent)
{
    build_ui();
}

AndBreakpointWindow::~AndBreakpointWindow()
{

}

void AndBreakpointWindow::build_ui()
{
    this->resize(395, 165);
    grid_layout = new QGridLayout(this);

    second_condition_label = new QLabel(this);
    grid_layout->addWidget(second_condition_label, 1, 0, 1, 1);

    first_condition_label = new QLabel(this);
    grid_layout->addWidget(first_condition_label, 0, 0, 1, 1);

    first_combo = new QComboBox(this);
    grid_layout->addWidget(first_combo, 0, 1, 1, 1);

    buttonBox = new QDialogButtonBox(this);
    buttonBox->setOrientation(Qt::Horizontal);
    buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);
    grid_layout->addWidget(buttonBox, 5, 0, 1, 4);

    second_combo = new QComboBox(this);
    grid_layout->addWidget(second_combo, 1, 1, 1, 1);

    verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);
    grid_layout->addItem(verticalSpacer, 2, 1, 1, 1);

    first_label = new QLabel(this);
    grid_layout->addWidget(first_label, 0, 2, 1, 1);

    second_label = new QLabel(this);
    grid_layout->addWidget(second_label, 1, 2, 1, 1);
    
    setWindowTitle(QApplication::translate("AndBreakpointWindow", "Dialog", nullptr));
    second_condition_label->setText(QApplication::translate("AndBreakpointWindow", "Second Condition", nullptr));
    first_condition_label->setText(QApplication::translate("AndBreakpointWindow", "First Condition", nullptr));
    first_label->setText(QApplication::translate("AndBreakpointWindow", "TextLabel", nullptr));
    second_label->setText(QApplication::translate("AndBreakpointWindow", "TextLabel", nullptr));

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    connect(first_combo, SIGNAL(activated(int)), this, SLOT(on_first_combo_activated(int)));
    connect(second_combo, SIGNAL(activated(int)), this, SLOT(on_second_combo_activated(int)));
}

DebuggerBreakpoint* AndBreakpointWindow::make_and_breakpoint()
{
    first_combo->addItem("Add Breakpoint...");
    second_combo->addItem("Add Breakpoint...");
    for(auto name : breakpoint_names)
    {
        first_combo->addItem(QString(name));
        second_combo->addItem(QString(name));
    }

    first_label->setText("");
    second_label->setText("");

    first_label->setFont((QFontDatabase::systemFont(QFontDatabase::FixedFont)));
    second_label->setFont((QFontDatabase::systemFont(QFontDatabase::FixedFont)));
    this->exec();

    if(this->result() == QDialog::Rejected || !_first || !_second)
    {
        QMessageBox messageBox;
        messageBox.critical(0,"Error","Failed to create breakpoint. Did you set something for both breakpoints?");
        delete _first;
        delete _second;
        return nullptr;
    }

    return new EmotionBreakpoints::And(_first, _second);
}

DebuggerBreakpoint* AndBreakpointWindow::make_and_breakpoint(DebuggerBreakpoint *first)
{
    first_combo->hide();
    second_combo->addItem("Add Breakpoint...");
    for(auto name : breakpoint_names)
    {
        first_combo->addItem(QString(name));
        second_combo->addItem(QString(name));
    }

    _first = first;

    if(_first)
        first_label->setText(QString(_first->get_description().c_str()));
    else
        first_label->setText("");

    second_label->setText("");

    first_label->setFont((QFontDatabase::systemFont(QFontDatabase::FixedFont)));
    second_label->setFont((QFontDatabase::systemFont(QFontDatabase::FixedFont)));
    this->exec();

    if(this->result() == QDialog::Rejected || !_first || !_second)
    {
        QMessageBox messageBox;
        messageBox.critical(0,"Error","Failed to create breakpoint. Did you set something for both breakpoints?");
        delete _first;
        delete _second;
        return nullptr;
    }

    return new EmotionBreakpoints::And(_first, _second);
}

DebuggerBreakpoint* AndBreakpointWindow::make_and_breakpoint(std::vector<EERegisterDefinition>* ee_regs)
{
    _ee_regs = ee_regs;
    setWindowTitle("And Breakpoint (EE)");
    return make_and_breakpoint();
}

DebuggerBreakpoint* AndBreakpointWindow::make_and_breakpoint(std::vector<IOPRegisterDefinition>* iop_regs)
{
    _iop_regs = iop_regs;
    setWindowTitle("And Breakpoint (IOP)");
    return make_and_breakpoint();
}

DebuggerBreakpoint* AndBreakpointWindow::make_and_breakpoint(std::vector<EERegisterDefinition>* ee_regs, DebuggerBreakpoint* first)
{
    _ee_regs = ee_regs;
    setWindowTitle("And Breakpoint (EE)");
    return make_and_breakpoint(first);
}

DebuggerBreakpoint* AndBreakpointWindow::make_and_breakpoint(std::vector<IOPRegisterDefinition>* iop_regs, DebuggerBreakpoint* first)
{
    _iop_regs = iop_regs;
    setWindowTitle("And Breakpoint (IOP)");
    return make_and_breakpoint(first);
}

void AndBreakpointWindow::on_first_combo_activated(int index)
{
    if(index > 0)
    {
        delete _first; // okay to do this, will either be null, or the last thing constructed
        _first = nullptr;

        if(_ee_regs)
        {
            _first = EEDebugWindow::create_breakpoint_prompt((BreakpointType)(index - 1), _ee_regs);
        }
        else if(_iop_regs)
        {
            _first = EEDebugWindow::create_breakpoint_prompt((BreakpointType)(index - 1), _iop_regs);
        }

        if(_first)
            first_label->setText(QString(_first->get_description().c_str()));
        else
            first_label->setText("");
    }
}

void AndBreakpointWindow::on_second_combo_activated(int index)
{
    if(index > 0)
    {
        delete _second; // okay to do this, will either be null, or the last thing constructed
        _second = nullptr;
        if(_ee_regs)
        {
            _second = EEDebugWindow::create_breakpoint_prompt((BreakpointType)(index - 1), _ee_regs);
        }
        else if(_iop_regs)
        {
            _second = EEDebugWindow::create_breakpoint_prompt((BreakpointType)(index - 1), _iop_regs);
        }

        if(_second)
            second_label->setText(QString(_second->get_description().c_str()));
        else
            second_label->setText("");
    }
}
