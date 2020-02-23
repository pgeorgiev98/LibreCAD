/*****************************************************************************/
/*  gcode.h - Gcode export plugin for LibreCAD                               */
/*                                                                           */
/*  Copyright (C) 2020 Petko Georgiev, petko.vas.georgiev@gmail.com          */
/*                                                                           */
/*  This library is free software, licensed under the terms of the GNU       */
/*  General Public License as published by the Free Software Foundation,     */
/*  either version 2 of the License, or (at your option) any later version.  */
/*  You should have received a copy of the GNU General Public License        */
/*  along with this program.  If not, see <http://www.gnu.org/licenses/>.    */
/*****************************************************************************/

#ifndef GCODE_H
#define GCODE_H

#include "qc_plugininterface.h"
#include <QDialog>
#include <QSettings>

class QSpinBox;
class QDoubleSpinBox;
class QPlainTextEdit;

class LC_Gcode : public QObject, QC_PluginInterface
{
    Q_OBJECT
    Q_INTERFACES(QC_PluginInterface)
    Q_PLUGIN_METADATA(IID LC_DocumentInterface_iid FILE  "gcode.json")

 public:
    virtual PluginCapabilities getCapabilities() const Q_DECL_OVERRIDE;
    virtual QString name() const Q_DECL_OVERRIDE;
    virtual void execComm(Document_Interface *doc,
                          QWidget *parent, QString cmd) Q_DECL_OVERRIDE;
};

class lc_Gcodedlg : public QDialog
{
    Q_OBJECT

public:
    explicit lc_Gcodedlg(Document_Interface *m_doc, QWidget *parent = nullptr);

    struct Line
    {
        QPointF a, b;
        Line() {}
        Line(QPointF a, QPointF b)
            : a(a), b(b) {}
    };

private slots:
    void readSettings();
    void writeSettings();
    void generateGcode();

private:
    Document_Interface *m_doc;
    QSettings m_settings;

    QPlainTextEdit *m_startingGcode;
    QPlainTextEdit *m_endingGcode;

    QSpinBox *m_feedrate;
    QSpinBox *m_zHopFeedrate;
    QSpinBox *m_travelFeedrate;

    QSpinBox *m_zHopHeight;

    QDoubleSpinBox *m_maxError;

    QVector<Line> m_lines;
    QByteArray m_gcode;
};

#endif // GCODE_H
