#include <QPainter>
#include <QImageWriter>
#include <QDateTime>
#include <QDebug>

#include "renderwidget.hpp"
#include "settings.hpp"

RenderWidget::RenderWidget(QWidget* parent)
    : QWidget(parent)
{
    QPalette palette;
    palette.setColor(QPalette::Window, Qt::black);
    setPalette(palette);
    setAutoFillBackground(true);
}

void RenderWidget::draw_frame(uint32_t* buffer, int inner_w, int inner_h, int final_w, int final_h)
{
    if (!buffer || !inner_w || !inner_h)
        return;

    final_image = QImage(
        reinterpret_cast<uint8_t*>(buffer),
        inner_w, inner_h, QImage::Format_RGBA8888
    ).scaled(final_w, final_h);

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

    QImage image = final_image.scaled(
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

    writer.write(final_image);
}
