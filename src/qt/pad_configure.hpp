#ifndef PAD_CONFIGURE_H
#define PAD_CONFIGURE_H

#include <QDialog>
#include "../input_common/common_input.hpp"


namespace Ui {
class pad_configure;
}

class pad_configure : public QDialog
{
    Q_OBJECT

public:
    explicit pad_configure(QWidget *parent = nullptr);
    ~pad_configure();

private:
    Ui::pad_configure *ui;
    QT_FEATURE_combobox Devices;
};

#endif // PAD_CONFIGURE_H
