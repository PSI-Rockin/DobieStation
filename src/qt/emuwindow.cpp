#include <cmath>
#include <fstream>
#include <iostream>

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

    connect(this, SIGNAL(shutdown()), &emuthread, SLOT(shutdown()));
    connect(this, SIGNAL(press_key(PAD_BUTTON)), &emuthread, SLOT(press_key(PAD_BUTTON)));
    connect(this, SIGNAL(release_key(PAD_BUTTON)), &emuthread, SLOT(release_key(PAD_BUTTON)));
    connect(&emuthread, SIGNAL(completed_frame(uint32_t*, int, int, int, int)),
            this, SLOT(draw_frame(uint32_t*, int, int, int, int)));
    connect(&emuthread, SIGNAL(update_FPS(int)), this, SLOT(update_FPS(int)));
    connect(&emuthread, SIGNAL(emu_error(QString)), this, SLOT(emu_error(QString)));
    emuthread.pause(PAUSE_EVENT::GAME_NOT_LOADED);

    emuthread.reset();
    emuthread.start();

    //Initialize window
    title = "DobieStation";
    setWindowTitle(QString::fromStdString(title));
    resize(640, 448);
    show();
}

int EmuWindow::init(int argc, char** argv)
{
    bool skip_BIOS = false;
    char* argv0; // Program name; AKA argv[0]

    char* bios_name = nullptr, *file_name = nullptr, *gsdump = nullptr;

    ARGBEGIN {
        case 'b':
            bios_name = ARGF();
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
    if (!BIOS_file.is_open())
    {
        printf("Failed to load PS2 BIOS from %s\n", bios_name);
        return 1;
    }
    printf("Loaded PS2 BIOS.\n");
    uint8_t* BIOS = new uint8_t[1024 * 1024 * 4];
    BIOS_file.read((char*)BIOS, 1024 * 1024 * 4);
    BIOS_file.close();
    emuthread.load_BIOS(BIOS);
    delete[] BIOS;
    BIOS = nullptr;

    if (file_name)
    {
        if (load_exec(file_name, skip_BIOS))
            return 1;
    }
    return 0;
}

int EmuWindow::run_gsdump(const char* file_name)
{
    emuthread.gsdump_read(file_name);
    emuthread.unpause(PAUSE_EVENT::GAME_NOT_LOADED);
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
        emuthread.load_ELF(ELF, ELF_size);
        delete[] ELF;
        ELF = nullptr;
        if (skip_BIOS)
            emuthread.set_skip_BIOS_hack(SKIP_HACK::LOAD_ELF);
    }
    else if (format == ".iso")
    {
        exec_file.close();
        emuthread.load_CDVD(file_name);
        if (skip_BIOS)
            emuthread.set_skip_BIOS_hack(SKIP_HACK::LOAD_DISC);
    }
    else
    {
        printf("Unrecognized file format %s\n", format.c_str());
        return 1;
    }

    title = file_name;
    ROM_path = file_name;

    emuthread.unpause(PAUSE_EVENT::GAME_NOT_LOADED);
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
        [this]() { this->emuthread.gsdump_write_toggle(); });

    file_menu = menuBar()->addMenu(tr("&File"));
    file_menu->addAction(load_rom_action);
    file_menu->addAction(load_bios_action);
    file_menu->addAction(load_state_action);
    file_menu->addAction(save_state_action);
    file_menu->addAction(exit_action);
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
        [this]() { this->emuthread.frame_advance ^= true; });
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
        resize(final_w*scale_factor, final_h*scale_factor);
    }
    update();
}

void EmuWindow::paintEvent(QPaintEvent *event)
{
    event->accept();
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);

    printf("Draw image!\n");

    painter.drawPixmap(0, 0, QPixmap::fromImage(final_image));
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
        case Qt::Key_Return:
            emit press_key(PAD_BUTTON::START);
            break;
        case Qt::Key_Space:
            emit press_key(PAD_BUTTON::SELECT);
            break;
        case Qt::Key_Period:
            emuthread.unpause(PAUSE_EVENT::FRAME_ADVANCE);
            break;
        case Qt::Key_F1:
            emuthread.gsdump_single_frame();
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
    emuthread.pause(PAUSE_EVENT::FILE_DIALOG);
    QString file_name = QFileDialog::getOpenFileName(this, tr("Open Rom"), "", tr("ROM Files (*.elf *.iso)"));
    load_exec(file_name.toStdString().c_str(), false);
    emuthread.unpause(PAUSE_EVENT::FILE_DIALOG);
}

void EmuWindow::open_file_skip()
{
    emuthread.pause(PAUSE_EVENT::FILE_DIALOG);
    QString file_name = QFileDialog::getOpenFileName(this, tr("Open Rom"), "", tr("ROM Files (*.elf *.iso)"));
    load_exec(file_name.toStdString().c_str(), true);
    emuthread.unpause(PAUSE_EVENT::FILE_DIALOG);
}

void EmuWindow::load_state()
{
    emuthread.pause(PAUSE_EVENT::FILE_DIALOG);
    string path = ROM_path.substr(0, ROM_path.length() - 4);
    path += ".snp";
    if (!emuthread.load_state(path.c_str()))
        printf("Failed to load %s\n", path.c_str());
    emuthread.unpause(PAUSE_EVENT::FILE_DIALOG);
}

void EmuWindow::save_state()
{
    emuthread.pause(PAUSE_EVENT::FILE_DIALOG);
    string path = ROM_path.substr(0, ROM_path.length() - 4);
    path += ".snp";
    if (!emuthread.save_state(path.c_str()))
        printf("Failed to save %s\n", path.c_str());
    emuthread.unpause(PAUSE_EVENT::FILE_DIALOG);
}
