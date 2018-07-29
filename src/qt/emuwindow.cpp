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
    if (argc < 2)
    {
        printf("Args: [BIOS] (Optional)[ELF/ISO]\n");
        return 1;
    }

    char* bios_name = argv[1];

    char* file_name;
    if (argc == 4)
        file_name = argv[2];
    else if (argc == 3 && strcmp(argv[2], "-skip") != 0)
        file_name = argv[2];
    else
        file_name = nullptr;

    bool skip_BIOS = false;
    //Flag parsing - to be reworked
    if (argc >= 3)
    {
        if (argc == 3)
        {
            if (strcmp(argv[2], "-skip") == 0)
                skip_BIOS = true;
        } 
        else if (argc == 4)
        {
            if (strcmp(argv[3], "-skip") == 0)
                skip_BIOS = true;
        }
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
    load_rom_action = new QAction(tr("&Load ROM... (Fast)"), this);
    connect(load_rom_action, &QAction::triggered, this, &EmuWindow::open_file_skip);

    load_bios_action = new QAction(tr("&Load ROM... (Boot BIOS)"), this);
    connect(load_bios_action, &QAction::triggered, this, &EmuWindow::open_file_no_skip);

    load_state_action = new QAction(tr("&Load State"), this);
    connect(load_state_action, &QAction::triggered, this, &EmuWindow::load_state);

    save_state_action = new QAction(tr("&Save State"), this);
    connect(save_state_action, &QAction::triggered, this, &EmuWindow::save_state);

    exit_action = new QAction(tr("&Exit"), this);
    connect(exit_action, &QAction::triggered, this, &QWidget::close);

    file_menu = menuBar()->addMenu(tr("&File"));
    file_menu->addAction(load_rom_action);
    file_menu->addAction(load_bios_action);
    file_menu->addAction(load_state_action);
    file_menu->addAction(save_state_action);
    file_menu->addAction(exit_action);

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
