#include "PlaceholderPlotWidget.h"

#include <QLabel>
#include <QVBoxLayout>

PlaceholderPlotWidget::PlaceholderPlotWidget(const QString& title, QWidget* parent) : QFrame(parent)
{
    setObjectName("placeholderPlot");
    setMinimumHeight(180);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);

    titleLabel_ = new QLabel(title, this);
    titleLabel_->setObjectName("placeholderTitle");

    messageLabel_ = new QLabel("Здесь будет график.\nПосле расчёта появятся подготовленные данные.", this);
    messageLabel_->setObjectName("placeholderMessage");
    messageLabel_->setWordWrap(true);

    layout->addWidget(titleLabel_);
    layout->addStretch();
    layout->addWidget(messageLabel_);
}

void PlaceholderPlotWidget::SetMessage(const QString& message)
{
    messageLabel_->setText(message);
}

