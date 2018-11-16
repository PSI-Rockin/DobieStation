#include <cmath>
#include <fstream>
#include <iostream>

#include <QApplication>
#include <QPainter>
#include <QString>
#include <QVBoxLayout>
#include <QMenuBar>
#include <QFileDialog>
#include <QMessageBox>

#include "emuwindow.hpp"

#include "arg.h"

using namespace std;

ifstream::pos_type filesize(const char* filename)
{
    ifstream in(filename, ifstream::ate | ifstream::binary);
    return in.tellg();
}

EmuWindow::EmuWindow(QWidget *parent) : QMainWindow(parent)
{
    old_frametime = chrono::system_clock::now();
    old_update_time = chrono::system_clock::now();
    framerate_avg = 0.0;
    scale_factor = 1;

    QWidget* widget = new QWidget;
    setCentralWidget(widget);

    QWidget* topFiller = new QWidget;
    topFiller->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QWidget *bottomFiller = new QWidget;
    bottomFiller->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->setMargin(5);
    layout->addWidget(topFiller);
    layout->addWidget(bottomFiller);
    widget->setLayout(layout);

    create_menu();

    connect(this, SIGNAL(shutdown()), &emu_thread, SLOT(shutdown()));
    connect(this, SIGNAL(press_key(PAD_BUTTON)), &emu_thread, SLOT(press_key(PAD_BUTTON)));
    connect(this, SIGNAL(release_key(PAD_BUTTON)), &emu_thread, SLOT(release_key(PAD_BUTTON)));
    connect(&emu_thread, SIGNAL(completed_frame(uint32_t*, int, int, int, int)),
            this, SLOT(draw_frame(uint32_t*, int, int, int, int)));
    connect(&emu_thread, SIGNAL(update_FPS(int)), this, SLOT(update_FPS(int)));
    connect(&emu_thread, SIGNAL(emu_error(QString)), this, SLOT(emu_error(QString)));
    connect(&emu_thread, SIGNAL(emu_non_fatal_error(QString)), this, SLOT(emu_non_fatal_error(QString)));
    emu_thread.pause(PAUSE_EVENT::GAME_NOT_LOADED);

    emu_thread.reset();
    emu_thread.start();

    //Initialize window
    title = "DobieStation";
    setWindowTitle(QString::fromStdString(title));
    if (menuBar()->isNativeMenuBar())
        resize(640, 448);
    else
        resize(640, 448 + menuBar()->geometry().height());
    show();
}

int EmuWindow::init(int argc, char** argv)
{
    bool skip_BIOS = false;
    char* argv0; // Program name; AKA argv[0]

    char* bios_name = nullptr, *file_name = nullptr, *gsdump = nullptr;

    // Load before the arguments, so the arguments override the config.
    Settings::load();
    if (!Settings::bios_path.isEmpty()) bios_name = Settings::as_char(Settings::bios_path);

    ARGBEGIN {
        case 'b':
            bios_name = ARGF();

            // Store the bios path, converting to a QString along the way.
            Settings::bios_path = QString::fromLocal8Bit(bios_name);
            break;
        case 'f':
            file_name = ARGF();
            break;
        case 's':
            skip_BIOS = true;
            break;
        case 'g':
            gsdump = ARGF();
            break;
        case 'h':
        default:
            printf("usage: %s [options]\n\n", argv0);
            printf("options:\n");
            printf("-b {BIOS}\tspecify BIOS\n");
            printf("-f {ELF/ISO}\tspecify ELF/ISO\n");
            printf("-h\t\tshow this message\n");
            printf("-s\t\tskip BIOS\n");
            printf("-g {.GSD}\t\trun a gsdump\n");
            return 1;
    } ARGEND

    if (gsdump)
    {
        return run_gsdump(gsdump);
    }

    ifstream BIOS_file(bios_name, ios::binary | ios::in);

    // Try to head off loading a bad bios file.
    if (!BIOS_file.is_open() || (filesize(bios_name) <= 0))
    {
        printf("Failed to load PS2 BIOS from %s\n", bios_name);
        return 1;
    }
    
    uint8_t* BIOS = new uint8_t[1024 * 1024 * 4];
    BIOS_file.read((char*)BIOS, 1024 * 1024 * 4);
    BIOS_file.close();

    // Was there an error reading the file?
    if (!BIOS_file.good())
    {
        printf("Failed to load PS2 BIOS from %s\n", bios_name);
        return 1;
    }

    emu_thread.load_BIOS(BIOS);
    delete[] BIOS;
    BIOS = nullptr;

    // Save at the end of init, so our arguments are saved.
    Settings::save();

    if (file_name)
    {
        if (load_exec(file_name, skip_BIOS))
            return 1;
    }
    return 0;
}

int EmuWindow::run_gsdump(const char* file_name)
{
    emu_thread.gsdump_read(file_name);
    emu_thread.unpause(PAUSE_EVENT::GAME_NOT_LOADED);
    return 0;
}
int EmuWindow::load_exec(const char* file_name, bool skip_BIOS)
{
    ifstream exec_file(file_name, ios::binary | ios::in);
    if (!exec_file.is_open())
    {
        printf("Failed to load %s\n", file_name);
        return 1;
    }

    //Basic file format detection
    string file_string = file_name;
    string format = file_string.substr(file_string.length() - 4);
    transform(format.begin(), format.end(), format.begin(), ::tolower);
    printf("%s\n", format.c_str());

    if (format == ".elf")
    {
        long long ELF_size = filesize(file_name);
        uint8_t* ELF = new uint8_t[ELF_size];
        exec_file.read((char*)ELF, ELF_size);
        exec_file.close();

        printf("Loaded %s\n", file_name);
        printf("Size: %lld\n", ELF_size);
        emu_thread.load_ELF(ELF, ELF_size);
        delete[] ELF;
        ELF = nullptr;
        if (skip_BIOS)
            emu_thread.set_skip_BIOS_hack(SKIP_HACK::LOAD_ELF);
    }
    else if (format == ".iso")
    {
        exec_file.close();
        emu_thread.load_CDVD(file_name);
        if (skip_BIOS)
            emu_thread.set_skip_BIOS_hack(SKIP_HACK::LOAD_DISC);
    }
    else
    {
        printf("Unrecognized file format %s\n", format.c_str());
        return 1;
    }

    title = file_name;
    ROM_path = file_name;

    emu_thread.unpause(PAUSE_EVENT::GAME_NOT_LOADED);
    return 0;
}

void EmuWindow::create_menu()
{
    load_rom_action = new QAction(tr("Load ROM... (&Fast)"), this);
    connect(load_rom_action, &QAction::triggered, this, &EmuWindow::open_file_skip);

    load_bios_action = new QAction(tr("Load ROM... (&Boot BIOS)"), this);
    connect(load_bios_action, &QAction::triggered, this, &EmuWindow::open_file_no_skip);

    load_state_action = new QAction(tr("&Load State"), this);
    connect(load_state_action, &QAction::triggered, this, &EmuWindow::load_state);

    save_state_action = new QAction(tr("&Save State"), this);
    connect(save_state_action, &QAction::triggered, this, &EmuWindow::save_state);

    exit_action = new QAction(tr("&Exit"), this);
    connect(exit_action, &QAction::triggered, this, &QWidget::close);

    auto gsdump_action = new QAction(tr("&GS dump toggle"), this);
    connect(gsdump_action, &QAction::triggered, this,
        [this]() { this->emu_thread.gsdump_write_toggle(); });

    file_menu = menuBar()->addMenu(tr("&File"));
    file_menu->addAction(load_rom_action);
    file_menu->addAction(load_bios_action);
    file_menu->addAction(load_state_action);
    file_menu->addAction(save_state_action);
    file_menu->addAction(exit_action);
    file_menu->addSeparator();
    file_menu->addAction(gsdump_action);

    options_menu = menuBar()->addMenu(tr("&Options"));
    auto size_options_actions = new QAction(tr("Scale to &Window (ignore aspect ratio)"), this);
    connect(size_options_actions, &QAction::triggered, this,
        [this]() { this->scale_factor = 0; });
    options_menu->addAction(size_options_actions);

    size_options_actions = new QAction(tr("Scale &1x"), this);
    connect(size_options_actions, &QAction::triggered, this,
        [this]() { this->scale_factor = 1; });
    options_menu->addAction(size_options_actions);

    size_options_actions = new QAction(tr("Scale &2x"), this);
    connect(size_options_actions, &QAction::triggered, this,
        [this]() { this->scale_factor = 2; });
    options_menu->addAction(size_options_actions);

    size_options_actions = new QAction(tr("Scale &3x"), this);
    connect(size_options_actions, &QAction::triggered, this,
        [this]() { this->scale_factor = 3; });
    options_menu->addAction(size_options_actions);

    size_options_actions = new QAction(tr("Scale &4x"), this);
    connect(size_options_actions, &QAction::triggered, this,
        [this]() { this->scale_factor = 4; });
    options_menu->addAction(size_options_actions);

    options_menu->addSeparator();

    auto frame_action = new QAction(tr("&Frame Advance (10x draws for gs dumps)"), this);
    connect(frame_action, &QAction::triggered, this,
        [this]() { this->emu_thread.frame_advance ^= true; });
    options_menu->addAction(frame_action);
}

void EmuWindow::draw_frame(uint32_t *buffer, int inner_w, int inner_h, int final_w, int final_h)
{
    if (!buffer || !inner_w || !inner_h)
        return;
    final_image = QImage((uint8_t*)buffer, inner_w, inner_h, QImage::Format_RGBA8888);
    if (scale_factor == 0)
    {
        auto size = this->size();
        final_image = final_image.scaled(size.width(), size.height());
    }
    else
    {
        final_image = final_image.scaled(final_w*scale_factor, final_h*scale_factor);
        if (menuBar()->isNativeMenuBar())
            resize(final_w*scale_factor, final_h*scale_factor);
        else
            resize(final_w*scale_factor, final_h*scale_factor + menuBar()->geometry().height());
    }
    update();
}

void EmuWindow::paintEvent(QPaintEvent *event)
{
    event->accept();
    QPainter painter(this);

    printf("Draw image!\n");

    if (menuBar()->isNativeMenuBar())
        painter.drawPixmap(0, 0, QPixmap::fromImage(final_image));
    else
        painter.drawPixmap(0, menuBar()->geometry().height(), QPixmap::fromImage(final_image));
}

void EmuWindow::closeEvent(QCloseEvent *event)
{
    emit shutdown();
    event->accept();
}

void EmuWindow::keyPressEvent(QKeyEvent *event)
{
    event->accept();
    switch (event->key())
    {
        case Qt::Key_Up:
            emit press_key(PAD_BUTTON::UP);
            break;
        case Qt::Key_Down:
            emit press_key(PAD_BUTTON::DOWN);
            break;
        case Qt::Key_Left:
            emit press_key(PAD_BUTTON::LEFT);
            break;
        case Qt::Key_Right:
            emit press_key(PAD_BUTTON::RIGHT);
            break;
        case Qt::Key_Z:
            emit press_key(PAD_BUTTON::CIRCLE);
            break;
        case Qt::Key_X:
            emit press_key(PAD_BUTTON::CROSS);
            break;
        case Qt::Key_A:
            emit press_key(PAD_BUTTON::TRIANGLE);
            break;
        case Qt::Key_S:
            emit press_key(PAD_BUTTON::SQUARE);
            break;
        case Qt::Key_Q:
            emit press_key(PAD_BUTTON::L1);
            break;
        case Qt::Key_W:
            emit press_key(PAD_BUTTON::R1);
            break;
        case Qt::Key_Return:
            emit press_key(PAD_BUTTON::START);
            break;
        case Qt::Key_Space:
            emit press_key(PAD_BUTTON::SELECT);
            break;
        case Qt::Key_Period:
            emu_thread.unpause(PAUSE_EVENT::FRAME_ADVANCE);
            break;
        case Qt::Key_F1:
            emu_thread.gsdump_single_frame();
            break;
    }
}

void EmuWindow::keyReleaseEvent(QKeyEvent *event)
{
    event->accept();
    switch (event->key())
    {
        case Qt::Key_Up:
            emit release_key(PAD_BUTTON::UP);
            break;
        case Qt::Key_Down:
            emit release_key(PAD_BUTTON::DOWN);
            break;
        case Qt::Key_Left:
            emit release_key(PAD_BUTTON::LEFT);
            break;
        case Qt::Key_Right:
            emit release_key(PAD_BUTTON::RIGHT);
            break;
        case Qt::Key_Z:
            emit release_key(PAD_BUTTON::CIRCLE);
            break;
        case Qt::Key_X:
            emit release_key(PAD_BUTTON::CROSS);
            break;
        case Qt::Key_A:
            emit release_key(PAD_BUTTON::TRIANGLE);
            break;
        case Qt::Key_S:
            emit release_key(PAD_BUTTON::SQUARE);
            break;
        case Qt::Key_Q:
            emit release_key(PAD_BUTTON::L1);
            break;
        case Qt::Key_W:
            emit release_key(PAD_BUTTON::R1);
            break;
        case Qt::Key_Return:
            emit release_key(PAD_BUTTON::START);
            break;
        case Qt::Key_Space:
            emit release_key(PAD_BUTTON::SELECT);
            break;
    }
}

void EmuWindow::update_FPS(int FPS)
{
    /*
    Updates window title every second
    Framerate displayed is the average framerate over 1 second
    */
    chrono::system_clock::time_point now = chrono::system_clock::now();
    chrono::duration<double> elapsed_update_seconds = now - old_update_time;
    if (elapsed_update_seconds.count() >= 1.0)
    {
        string new_title = "FPS: " + to_string(FPS) + " - " + title;
        setWindowTitle(QString::fromStdString(new_title));
        old_update_time = chrono::system_clock::now();
    }
}

void EmuWindow::emu_error(QString err)
{
    QMessageBox msgBox;
    msgBox.setText("Emulation has been terminated");
    msgBox.setInformativeText(err);
    msgBox.setStandardButtons(QMessageBox::Abort);
    msgBox.setDefaultButton(QMessageBox::Abort);
    msgBox.exec();
    ROM_path = "";
    setWindowTitle("DobieStation");
}

void EmuWindow::emu_non_fatal_error(QString err)
{
    QMessageBox msgBox;
    msgBox.setText("Error");
    msgBox.setInformativeText(err);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.setDefaultButton(QMessageBox::Ok);
    msgBox.exec();
    emu_thread.unpause(MESSAGE_BOX);
}

#ifndef QT_NO_CONTEXTMENU
void EmuWindow::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);
    menu.addAction(load_rom_action);
    menu.addAction(load_bios_action);
    menu.addAction(exit_action);
    menu.exec(event->globalPos());
}
#endif // QT_NO_CONTEXTMENU

void EmuWindow::open_file_no_skip()
{
    emu_thread.pause(PAUSE_EVENT::FILE_DIALOG);
    QString file_name = QFileDialog::getOpenFileName(this, tr("Open Rom"), "", tr("ROM Files (*.elf *.iso)"));
    load_exec(file_name.toStdString().c_str(), false);
    emu_thread.unpause(PAUSE_EVENT::FILE_DIALOG);
}

void EmuWindow::open_file_skip()
{
    emu_thread.pause(PAUSE_EVENT::FILE_DIALOG);
    QString file_name = QFileDialog::getOpenFileName(this, tr("Open Rom"), "", tr("ROM Files (*.elf *.iso)"));
    load_exec(file_name.toStdString().c_str(), true);
    emu_thread.unpause(PAUSE_EVENT::FILE_DIALOG);
}

void EmuWindow::load_state()
{
    emu_thread.pause(PAUSE_EVENT::FILE_DIALOG);
    string path = ROM_path.substr(0, ROM_path.length() - 4);
    path += ".snp";
    if (!emu_thread.load_state(path.c_str()))
        printf("Failed to load %s\n", path.c_str());
    emu_thread.unpause(PAUSE_EVENT::FILE_DIALOG);
}

void EmuWindow::save_state()
{
    emu_thread.pause(PAUSE_EVENT::FILE_DIALOG);
    string path = ROM_path.substr(0, ROM_path.length() - 4);
    path += ".snp";
    if (!emu_thread.save_state(path.c_str()))
        printf("Failed to save %s\n", path.c_str());
    emu_thread.unpause(PAUSE_EVENT::FILE_DIALOG);
}
