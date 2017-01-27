#ifndef ROUNDSSELECTIONWIDGET_H
#define ROUNDSSELECTIONWIDGET_H

#include <QWidget>

namespace Ui {
class RoundsSelectionWidget;
}

class RoundsSelectionWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RoundsSelectionWidget(QWidget *parent = 0);
    ~RoundsSelectionWidget();
    void setRounds(int rounds);
    int getRounds();

private Q_SLOTS:
    void transformRoundsBenchmark();

private:
    Ui::RoundsSelectionWidget *ui;
};

#endif // ROUNDSSELECTIONWIDGET_H
