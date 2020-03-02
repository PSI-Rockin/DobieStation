#ifndef RENDERWIDGET_HPP
#define RENDERWIDGET_HPP

#include <QWidget>
#include <QPaintEvent>

class RenderWidget : public QWidget
{
    Q_OBJECT
    private:
        QImage final_image;
        bool respect_aspect_ratio = true;
    public:
        static const int MAX_SCALING = 4;
        static const int DEFAULT_WIDTH = 640;
        static const int DEFAULT_HEIGHT = 480;

        explicit RenderWidget(QWidget* parent = nullptr);
        void paintEvent(QPaintEvent* event) override;

        bool get_respect_aspect_ratio() const;
    public slots:
        void draw_frame(uint32_t* buffer, int inner_w, int inner_h, int final_w, int final_h);
        void toggle_aspect_ratio();
        void screenshot();
};
#endif