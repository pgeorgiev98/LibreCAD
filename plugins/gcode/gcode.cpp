/*****************************************************************************/
/*  gcode.cpp - Gcode export plugin for LibreCAD                             */
/*                                                                           */
/*  Copyright (C) 2020 Petko Georgiev, petko.vas.georgiev@gmail.com          */
/*                                                                           */
/*  This library is free software, licensed under the terms of the GNU       */
/*  General Public License as published by the Free Software Foundation,     */
/*  either version 2 of the License, or (at your option) any later version.  */
/*  You should have received a copy of the GNU General Public License        */
/*  along with this program.  If not, see <http://www.gnu.org/licenses/>.    */
/*****************************************************************************/

#include <QGridLayout>
#include <QPushButton>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPlainTextEdit>
#include <QLabel>
#include <QFileDialog>
#include <QFile>
#include <QtMath>

#include "document_interface.h"
#include "gcode.h"

#include <QDebug>

static const char *defaultStartingGcode =
        "G28 ;Home\n"
        "G90 ;Absolute positioning\n";

static const char *defaultEndingGcode =
        "G91 ;Relative positioning\n"
        "G0 Z10 ;Raise Z\n"
        "G90 ;Absolute positioning\n";

static const int defaultFeedrate = 600;
static const int defaultZHopFeedrate = 1800;
static const int defaultTravelFeedrate = 3000;
static const int defaultZHopHeight = 50;
static const double defaultMaxError = 0.01;

QString LC_Gcode::name() const
{
    return (tr("Gcode plugin"));
}

PluginCapabilities LC_Gcode::getCapabilities() const
{
    PluginCapabilities pluginCapabilities;
    pluginCapabilities.menuEntryPoints
            << PluginMenuLocation("plugins_menu", tr("Gcode plugin"));
    return pluginCapabilities;
}

void LC_Gcode::execComm(Document_Interface *doc,
                        QWidget *parent, QString cmd)
{
    Q_UNUSED(cmd);
    lc_Gcodedlg dialog(doc, parent);
    dialog.exec();
}




/*****************************/
lc_Gcodedlg::lc_Gcodedlg(Document_Interface *doc, QWidget *parent)
    : QDialog(parent)
    , m_doc(doc)
    , m_settings(QSettings::IniFormat, QSettings::UserScope, "LibreCAD", "gcode_plugin")
    , m_startingGcode(new QPlainTextEdit)
    , m_endingGcode(new QPlainTextEdit)
    , m_feedrate(new QSpinBox)
    , m_zHopFeedrate(new QSpinBox)
    , m_travelFeedrate(new QSpinBox)
    , m_zHopHeight(new QSpinBox)
    , m_maxError(new QDoubleSpinBox)
    , m_repetitions(new QSpinBox)
{
    m_feedrate->setButtonSymbols(QSpinBox::NoButtons);
    m_zHopFeedrate->setButtonSymbols(QSpinBox::NoButtons);
    m_travelFeedrate->setButtonSymbols(QSpinBox::NoButtons);
    m_zHopHeight->setButtonSymbols(QSpinBox::NoButtons);
    m_maxError->setButtonSymbols(QDoubleSpinBox::NoButtons);

    m_feedrate->setRange(1, 1000000000);
    m_zHopFeedrate->setRange(1, 1000000000);
    m_travelFeedrate->setRange(1, 1000000000);
    m_zHopHeight->setRange(-1000000000, 1000000000);
    m_maxError->setRange(0.000001, 1000000000);
    m_maxError->setDecimals(6);
    m_repetitions->setRange(1, 1000000000);

    setWindowTitle(tr("Generate Gcode"));

    QVBoxLayout *mainLayout = new QVBoxLayout;

    QPushButton *generateGcodeButton = new QPushButton("Generate Gcode");

    QGridLayout *gcodeLayout = new QGridLayout;
    gcodeLayout->addWidget(new QLabel("Starting Gcode:"), 0, 0, Qt::AlignHCenter);
    gcodeLayout->addWidget(new QLabel("Ending Gcode:"), 0, 1, Qt::AlignHCenter);
    gcodeLayout->addWidget(m_startingGcode, 1, 0);
    gcodeLayout->addWidget(m_endingGcode, 1, 1);

    QGridLayout *settingsLayout = new QGridLayout;
    int row = 0;
    settingsLayout->addWidget(new QLabel("Feedrate: "), row, 0);
    settingsLayout->addWidget(m_feedrate, row, 1);
    settingsLayout->addWidget(new QLabel(" mm/s"), row, 2);
    ++row;
    settingsLayout->addWidget(new QLabel("ZHop feedrate: "), row, 0);
    settingsLayout->addWidget(m_zHopFeedrate, row, 1);
    settingsLayout->addWidget(new QLabel(" mm/s"), row, 2);
    ++row;
    settingsLayout->addWidget(new QLabel("Travel feedrate: "), row, 0);
    settingsLayout->addWidget(m_travelFeedrate, row, 1);
    settingsLayout->addWidget(new QLabel(" mm/s"), row, 2);
    ++row;
    settingsLayout->addWidget(new QLabel("ZHop height: "), row, 0);
    settingsLayout->addWidget(m_zHopHeight, row, 1);
    settingsLayout->addWidget(new QLabel(" mm"), row, 2);
    ++row;
    settingsLayout->addWidget(new QLabel("Maximum error: "), row, 0);
    settingsLayout->addWidget(m_maxError, row, 1);
    settingsLayout->addWidget(new QLabel(" mm"), row, 2);
    ++row;
    settingsLayout->addWidget(new QLabel("Repeat "), row, 0);
    settingsLayout->addWidget(m_repetitions, row, 1);
    settingsLayout->addWidget(new QLabel(" times"), row, 2);

    mainLayout->addLayout(gcodeLayout);
    mainLayout->addSpacing(16);
    mainLayout->addLayout(settingsLayout);
    mainLayout->addSpacing(16);
    mainLayout->addWidget(generateGcodeButton);

    connect(generateGcodeButton, &QPushButton::clicked, this, &lc_Gcodedlg::generateGcode);

    setLayout(mainLayout);
    readSettings();
}


static QVector<lc_Gcodedlg::Line> arcToLines(QPointF center, double radius, double error, double startAngle, double endAngle, bool reversed)
{
    QVector<lc_Gcodedlg::Line> lines;

    if (reversed)
        qSwap(startAngle, endAngle);
    double deltaAngle = endAngle - startAngle;
    if (deltaAngle < 0)
        deltaAngle += 2 * M_PI;

    int n = qMax(3, qCeil(M_PI/acos(radius/(error+radius))));
    double outrad = radius / qCos(M_PI/n);
    double realError = outrad - radius;
    outrad -= realError / 2;
    auto pointAtAngle = [center, outrad](double angle) {
        return QPointF(center.x() + outrad * qCos(angle),
                       center.y() + outrad * qSin(angle));
    };
    int actualN = qMax(3, qCeil(n * deltaAngle / (2 * M_PI)));
    QPointF prev = pointAtAngle(startAngle);
    for (int i = 1; i <= actualN; ++i) {
        QPointF p = pointAtAngle(startAngle + i * deltaAngle / actualN);
        lines.append(lc_Gcodedlg::Line(prev, p));
        prev = p;
    }
    return lines;
}

static QVector<lc_Gcodedlg::Line> ellipseToLines(QPointF center, QVector2D a, double ratio, double error)
{
    QVector<lc_Gcodedlg::Line> lines;
    QVector2D b = ratio * QVector2D(-a.x(), a.y());
    double radA = a.length();
    double radB = b.length();
    double largerRad = qMax(radA, radB);
    int n = qMax(3, qCeil(M_PI/acos(largerRad/(error+largerRad))));
    double outradA = radA / qCos(M_PI/n);
    double realErrorA = outradA - radA;
    double outradB = radB / qCos(M_PI/n);
    double realErrorB = outradB - radB;
    auto pointAtAngle = [center, a, b, realErrorA, realErrorB](double angle) {
        return center + (qCos(angle) * (a + a.normalized() * realErrorA / 2) +
                         qSin(angle) * (b + b.normalized() * realErrorB / 2)).toPointF();
    };
    QPointF prev = pointAtAngle(0);
    for (int i = 1; i <= n; ++i) {
        double angle = i * 2 * M_PI / n;
        QPointF p = pointAtAngle(angle);
        lines.append(lc_Gcodedlg::Line(prev, p));
        prev = p;
    }
    return lines;
}

void lc_Gcodedlg::generateGcode()
{
    writeSettings();
    qDebug() << "Generating Gcode";

    int feedrate = m_feedrate->value();
    int zhopfeed = m_zHopFeedrate->value();
    int travelfeed = m_travelFeedrate->value();
    int zhopheight = m_zHopHeight->value();
    double maxError = m_maxError->value();

    m_gcode = m_startingGcode->toPlainText().toLatin1();
    if (!m_gcode.endsWith('\n'))
        m_gcode.append('\n');

    QList<Plug_Entity *> selection;
    if (!m_doc->getAllEntities(&selection, true)) {
        qDebug() << "Get entities failed";
        return;
    }

    qDebug() << "Got" << selection.size() << "entities";
    m_lines.clear();
    for (auto entity : selection) {
        QHash<int, QVariant> data;
        entity->getData(&data);
        switch(data.value(DPI::ETYPE).toInt()) {
        case DPI::LINE: {
            QPointF a, b;
            a.setX(data.value(DPI::STARTX).toDouble());
            a.setY(data.value(DPI::STARTY).toDouble());
            b.setX(data.value(DPI::ENDX).toDouble());
            b.setY(data.value(DPI::ENDY).toDouble());
            m_lines.append(Line(a, b));
            break;
        }
        case DPI::CIRCLE: {
            QPointF center;
            center.setX(data.value(DPI::STARTX).toDouble());
            center.setY(data.value(DPI::STARTY).toDouble());
            double radius = data.value(DPI::RADIUS).toDouble();
            m_lines.append(arcToLines(center, radius, maxError, 0, 2 * M_PI, false));
            break;
        }
        case DPI::ARC: {
            QPointF center;
            center.setX(data.value(DPI::STARTX).toDouble());
            center.setY(data.value(DPI::STARTY).toDouble());
            double radius = data.value(DPI::RADIUS).toDouble();
            double startAngle = data.value(DPI::STARTANGLE).toDouble();
            double endAngle = data.value(DPI::ENDANGLE).toDouble();
            bool reversed = data.value(DPI::REVERSED).toBool();
            m_lines.append(arcToLines(center, radius, maxError, startAngle, endAngle, reversed));
            break;
        }
        case DPI::ELLIPSE: {
            QPointF center;
            QVector2D point;
            center.setX(data.value(DPI::STARTX).toDouble());
            center.setY(data.value(DPI::STARTY).toDouble());
            point.setX(data.value(DPI::ENDX).toDouble());
            point.setY(data.value(DPI::ENDY).toDouble());
            double ratio = data.value(DPI::HEIGHT).toDouble();
            m_lines.append(ellipseToLines(center, point, ratio, maxError));
            break;
        }
        default:
            qDebug() << "Unsupported entity";
        }
    }

    QVector<Line> lines = findBestPath(m_lines);

    QPointF currentPosition;;
    for (int i = 0; i < m_repetitions->value(); ++i) {
        for (Line l : lines) {
            qDebug().noquote() << QString("Line from (%1, %2) to (%3, %4); current: (%5, %6)").arg(l.a.x()).arg(l.a.y()).arg(l.b.x()).arg(l.b.y()).arg(currentPosition.x()).arg(currentPosition.y());
            if (currentPosition == l.b)
                qSwap(l.a, l.b);
            if (currentPosition != l.a || currentPosition.isNull()) {
                m_gcode.append(QString("G0 Z%1 F%2\n").arg(zhopheight).arg(zhopfeed));
                m_gcode.append(QString("G0 X%1 Y%2 F%3\n").arg(l.a.x()).arg(l.a.y()).arg(travelfeed));
                m_gcode.append(QString("G0 Z0 F%1\n").arg(zhopfeed));
            }
            m_gcode.append(QString("G1 X%1 Y%2 F%3\n").arg(l.b.x()).arg(l.b.y()).arg(feedrate));
            currentPosition = l.b;
        }
    }

    m_gcode.append(m_endingGcode->toPlainText().toLatin1());

    QString name = QFileDialog::getSaveFileName(this, QString(), QString());
    if (name.isEmpty())
        return;
    QFile file(name);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "Failed to open file:" << file.errorString();
        return;
    }
    file.write(m_gcode);
    file.close();
}


/*
bool lc_Gcodedlg::failGUI(QString *msg)
{
    if (startxedit->text().isEmpty()) {msg->insert(0, tr("Start X is empty")); return true;}
    if (startyedit->text().isEmpty()) {msg->insert(0, tr("Start Y is empty")); return true;}
    if (endxedit->text().isEmpty()) {msg->insert(0, tr("End X is empty")); return true;}
    if (endyedit->text().isEmpty()) {msg->insert(0, tr("End Y is empty")); return true;}
    return false;
}
*/


/*
void lc_Gcodedlg::processAction(Document_Interface *doc)
{
    Q_UNUSED(doc);
    QPointF start, end;
    start.setX(startxedit->text().toDouble());
    start.setY(startyedit->text().toDouble());
    end.setX(endxedit->text().toDouble());
    end.setY(endyedit->text().toDouble());

    doc->addLine(&start, &end);
}
*/

/*
void lc_Gcodedlg::checkAccept()
{

    errmsg.clear();
    if (failGUI(&errmsg)) {
        QMessageBox::critical ( this, tr("Gcode plugin"), errmsg );
        errmsg.clear();
        return;
    }
    accept();
}
*/


void lc_Gcodedlg::readSettings()
{
    QPoint pos = m_settings.value("pos", QPoint(200, 200)).toPoint();
    QSize size = m_settings.value("size", QSize(430,140)).toSize();

    m_startingGcode->setPlainText(m_settings.value("starting_gcode", defaultStartingGcode).toString());
    m_endingGcode->setPlainText(m_settings.value("ending_gcode", defaultEndingGcode).toString());
    m_feedrate->setValue(m_settings.value("feedrate", defaultFeedrate).toInt());
    m_zHopFeedrate->setValue(m_settings.value("zhop_feedrate", defaultZHopFeedrate).toInt());
    m_travelFeedrate->setValue(m_settings.value("travel_feedrate", defaultTravelFeedrate).toInt());
    m_zHopHeight->setValue(m_settings.value("zhop_height", defaultZHopHeight).toInt());
    m_maxError->setValue(m_settings.value("max_error", defaultMaxError).toDouble());
    m_repetitions->setValue(m_settings.value("repetitions", 1).toInt());

    resize(size);
    move(pos);
}

void lc_Gcodedlg::writeSettings()
{
    qDebug() << "Write settings";
    m_settings.setValue("pos", pos());
    m_settings.setValue("size", size());

    if (m_startingGcode->toPlainText() != m_settings.value("starting_gcode").toString())
        m_settings.setValue("starting_gcode", m_startingGcode->toPlainText());
    if (m_endingGcode->toPlainText() != m_settings.value("ending_gcode").toString())
        m_settings.setValue("ending_gcode", m_endingGcode->toPlainText());
    m_settings.setValue("feedrate", m_feedrate->value());
    m_settings.setValue("zhop_feedrate", m_zHopFeedrate->value());
    m_settings.setValue("travel_feedrate", m_travelFeedrate->value());
    m_settings.setValue("zhop_height", m_zHopHeight->value());
    m_settings.setValue("max_error", m_maxError->value());
    m_settings.setValue("repetitions", m_repetitions->value());
}

QVector<lc_Gcodedlg::Line> lc_Gcodedlg::findBestPath(const QVector<Line> &lines) const
{
    struct Node
    {
        QPointF point;
        // The next node index and the distance to it
        QVector<int> nextNodes;
    };
    struct Graph
    {
        QVector<Node> nodes;
        const double epsilon;
        Graph(double epsilon) : epsilon(epsilon) {}

        // Gets the index of the node at `point`
        // If it does not exist, it is added to the graph
        int getNode(QPointF point)
        {
            for (int i = 0; i < nodes.size(); ++i)
                if ((nodes[i].point - point).manhattanLength() < epsilon)
                    return i;
            nodes.append(Node{point, {}});
            return nodes.size() - 1;
        }

        // Builds the graph
        void addLines(const QVector<Line> &lines)
        {
            for (Line line : lines) {
                int n1 = getNode(line.a);
                int n2 = getNode(line.b);
                if (n1 != n2) {
                    nodes[n1].nextNodes.append(n2);
                    nodes[n2].nextNodes.append(n1);
                }
            }
        }
    } graph(m_maxError->value());

    graph.addLines(lines);

    QVector<Line> solution;

    for (;;) {
        // Find the best starting position
        // That is the closest to (0,0) leaf
        // If no leaves exist, then the closest node is chosen
        int start = -1;
        {
            int s1 = -1, s2 = -1;
            double l1 = qInf(), l2 = qInf();
            for (int i = 0; i < graph.nodes.size(); ++i) {
                const Node &n = graph.nodes[i];
                double l = QLineF(QPointF(0, 0), n.point).length();
                if (n.nextNodes.size() == 1) {
                    if (l < l1) {
                        l1 = l;
                        s1 = i;
                    }
                } else if (!n.nextNodes.isEmpty()) {
                    if (l < l2) {
                        l2 = l;
                        s2 = i;
                    }
                }
            }
            if (s1 != -1)
                start = s1;
            else if (s2 != -1)
                start = s2;
            else
                break;
        }

        // Make a path by just choosing the first adjacent node (TODO?)
        // The path is added to `solution` and all of its edges are removed from the graph
        int current = start;
        for (;;) {
            Node &node = graph.nodes[current];
            int next = -1;
            if (node.nextNodes.isEmpty())
                break;
            next = node.nextNodes.first();
            node.nextNodes.removeOne(next);
            graph.nodes[next].nextNodes.removeOne(current);
            solution.append(Line(node.point, graph.nodes[next].point));
            current = next;
        }
    }

    return solution;
}
