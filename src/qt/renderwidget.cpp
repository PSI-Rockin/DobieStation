#include <QPainter>
#include <QImageWriter>
#include <QDateTime>
#include <QColor>
#include <QDebug>

#include "renderwidget.hpp"
#include "settings.hpp"
#include "gsdebugger.hpp"

RenderWidget::RenderWidget(QWidget* parent)
    : QWidget(parent)
{
    QPalette palette;
    palette.setColor(QPalette::Background, Qt::black);
    setPalette(palette);
    setAutoFillBackground(true);
}

void RenderWidget::draw_frame(uint32_t* buffer, int inner_w, int inner_h, int final_w, int final_h)
{
    if (!buffer || !inner_w || !inner_h)
        return;


    gs_framebuffer = QImage(
        reinterpret_cast<uint8_t*>(buffer),
        inner_w, inner_h, QImage::Format_RGBA8888
    ).scaled(final_w, final_h);

    // Not ideal
    output = gs_framebuffer;

    update();
}

void RenderWidget::paintEvent(QPaintEvent* event)
{
    event->accept();

    QPainter painter(this);

    QRect widget_rect(
        0, 0,
        painter.device()->width(),
        painter.device()->height()
    );

    QImage image = output.scaled(
        widget_rect.width(), widget_rect.height(),
        respect_aspect_ratio ? Qt::KeepAspectRatio : Qt::IgnoreAspectRatio
    );

    QRect src_rect(image.rect());
    src_rect.moveCenter(widget_rect.center());

    painter.drawImage(src_rect.topLeft(), image);
}

void RenderWidget::toggle_aspect_ratio()
{
    respect_aspect_ratio = !respect_aspect_ratio;
}

bool RenderWidget::get_respect_aspect_ratio() const
{
    return respect_aspect_ratio;
}

void RenderWidget::screenshot()
{
    QString file_name(
        QDateTime::currentDateTime()
            .toString("yyyyMMddHHmmsszzz")
            .append(".png")
    );

    QString directory(Settings::instance().screenshot_directory);

    QString output_file(
        directory.append(QDir::separator()).append(file_name)
    );

    QImageWriter writer(output_file, "png");

    if (!writer.canWrite())
    {
        qDebug() << "Could not take screenshot!";
        return;
    }

    writer.write(gs_framebuffer);
}

void RenderWidget::change_channel(int channel)
{
    for (int y = 0; y < output.height(); y++)
    {
        for (int x = 0; x < output.width(); x++)
        {
            QColor color = gs_framebuffer.pixelColor(x, y);

            QColor channel_color;
            switch (channel)
            {
            case ImageDock::Channel::RED:
                channel_color = QColor(color.red(), color.red(), color.red(), 255);
                break;
            case ImageDock::Channel::GREEN:
                channel_color = QColor(color.green(), color.green(), color.green(), 255);
                break;
            case ImageDock::Channel::BLUE:
                channel_color = QColor(color.blue(), color.blue(), color.blue(), 255);
                break;
            default:
                channel_color = color;
            }

            output.setPixelColor(x, y, channel_color);
        }
    }

    update();
}

void RenderWidget::clear_screen()
{
    output.fill(Qt::black);
    update();
}