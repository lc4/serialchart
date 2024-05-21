#ifndef CHART_H
#define CHART_H

#include <QWidget>
#include "common.h"
#include "configuration.h"
#include "decoderbase.h"

class Chart : public QWidget
{
Q_OBJECT
public:
    explicit Chart(QWidget *parent = 0);
    void init(Configuration* config);
private:
    Configuration* config;
    QPixmap px;
	QPixmap bg;
    int cursor_pos;
    QList<QPoint> prevPoints;
signals:

public slots:
    void newPacket(DecoderBase* decoder);

protected:
    void paintEvent(QPaintEvent* ev);

};

#endif // CHART_H
