#include <QtWidgets/QMessageBox>
#include <QtGui/QFontDatabase>
#include "and_breakpoint_window.hpp"
#include "ui_and_breakpoint_window.h"
#include "ee_debugwindow.hpp"


static const char* breakpoint_names[] = {"And", "Memory", "Register", "PC", "Step"};

AndBreakpointWindow::AndBreakpointWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AndBreakpointWindow)
{
    ui->setupUi(this);
}

AndBreakpointWindow::~AndBreakpointWindow()
{
    delete ui;
}

DebuggerBreakpoint* AndBreakpointWindow::make_and_breakpoint()
{
    ui->first_combo->addItem("Add Breakpoint...");
    ui->second_combo->addItem("Add Breakpoint...");
    for(auto name : breakpoint_names)
    {
        ui->first_combo->addItem(QString(name));
        ui->second_combo->addItem(QString(name));
    }

    ui->first_label->setText("");
    ui->second_label->setText("");

    ui->first_label->setFont((QFontDatabase::systemFont(QFontDatabase::FixedFont)));
    ui->second_label->setFont((QFontDatabase::systemFont(QFontDatabase::FixedFont)));
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
    ui->first_combo->hide();
    ui->second_combo->addItem("Add Breakpoint...");
    for(auto name : breakpoint_names)
    {
        ui->first_combo->addItem(QString(name));
        ui->second_combo->addItem(QString(name));
    }

    _first = first;

    if(_first)
        ui->first_label->setText(QString(_first->get_description().c_str()));
    else
        ui->first_label->setText("");

    ui->second_label->setText("");

    ui->first_label->setFont((QFontDatabase::systemFont(QFontDatabase::FixedFont)));
    ui->second_label->setFont((QFontDatabase::systemFont(QFontDatabase::FixedFont)));
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
            ui->first_label->setText(QString(_first->get_description().c_str()));
        else
            ui->first_label->setText("");
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
            ui->second_label->setText(QString(_second->get_description().c_str()));
        else
            ui->second_label->setText("");
    }
}
