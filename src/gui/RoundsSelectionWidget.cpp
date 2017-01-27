#include "RoundsSelectionWidget.h"
#include "ui_RoundsSelectionWidget.h"

#include "keys/CompositeKey.h"

RoundsSelectionWidget::RoundsSelectionWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::RoundsSelectionWidget)
{
    ui->setupUi(this);
    connect(ui->transformBenchmarkButton, SIGNAL(clicked()), SLOT(transformRoundsBenchmark()));

}

RoundsSelectionWidget::~RoundsSelectionWidget()
{
    delete ui;
}

void RoundsSelectionWidget::transformRoundsBenchmark()
{
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    int seconds = ui->transformRoundsSecondsSpinBox->value();
    if (seconds > 0) {
      int rounds = CompositeKey::transformKeyBenchmark(seconds * 1000);
      if (rounds != -1) {
          ui->transformRoundsSpinBox->setValue(rounds);
      }
    }
    QApplication::restoreOverrideCursor();
}

void RoundsSelectionWidget::setRounds(int rounds)
{
  ui->transformRoundsSpinBox->setValue(rounds);
}

int RoundsSelectionWidget::getRounds()
{
  return ui->transformRoundsSpinBox->value();
}
