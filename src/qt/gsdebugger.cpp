#include <QPushButton>
#include <QLineEdit>
#include <QGridLayout>
#include <QComboBox>
#include <QLabel>
#include <QDebug>

#include "gsdebugger.hpp"

GSControlDock::GSControlDock(QWidget* parent)
    : QDockWidget(tr("GS Control"))
{
    auto goto_draw_label = new QPushButton(tr("GOTO Draw"));
    auto goto_draw = new QLineEdit;

    goto_draw->setText("0");
    goto_draw->setInputMask("D000");

    
    connect(goto_draw_label, &QAbstractButton::clicked, [=]() {
        emit request_goto_draw(goto_draw->text().toInt());
    });

    auto next_draw_button = new QPushButton(tr("Next Draw"));
    connect(next_draw_button, &QAbstractButton::clicked, [=]() {
        emit request_single_draw();
    });
    auto next_ten_draw_button = new QPushButton(tr("Next 10 Draws"));
    connect(next_ten_draw_button, &QAbstractButton::clicked, [=]() {
        emit request_ten_draws();
    });

    auto next_frame_button = new QPushButton(tr("Next Frame"));
    connect(next_frame_button, &QAbstractButton::clicked, [=]() {
        emit request_next_frame();
    });
    auto draw_frame_button = new QPushButton(tr("Draw Frame"));
    connect(draw_frame_button, &QAbstractButton::clicked, [=]() {
        emit request_draw_frame();
    });

    auto control_layout = new QGridLayout;
    control_layout->addWidget(goto_draw, 0, 0);
    control_layout->addWidget(goto_draw_label, 0, 1);
    control_layout->addWidget(next_draw_button, 1, 0);
    control_layout->addWidget(next_ten_draw_button, 1, 1);
    control_layout->addWidget(next_frame_button, 2, 0);
    control_layout->addWidget(draw_frame_button, 2, 1);
    control_layout->setAlignment(Qt::AlignTop);

    auto gs_control_widget = new QWidget;
    gs_control_widget->setLayout(control_layout);

    setWidget(gs_control_widget);
}

ImageDock::ImageDock(QWidget* parent)
    : QDockWidget(tr("Image Control"))
{
    auto channel_label = new QLabel(tr("Channel:"));
    auto channel_select = new QComboBox;
    channel_select->addItem("All", QVariant(Channel::ALL));
    channel_select->addItem("Red", QVariant(Channel::RED));
    channel_select->addItem("Green", QVariant(Channel::GREEN));
    channel_select->addItem("Blue", QVariant(Channel::BLUE));

    connect(
        channel_select, QOverload<int>::of(&QComboBox::activated),
        this, &ImageDock::request_channel
    );
    
    auto control_layout = new QGridLayout;
    control_layout->addWidget(channel_label, 0, 0);
    control_layout->addWidget(channel_select, 0, 1);
    control_layout->setAlignment(Qt::AlignTop);

    auto widget = new QWidget;
    widget->setLayout(control_layout);

    setWidget(widget);
}