#include "serialtool.h"
#include "portsetbox.h"
#include "optionsbox.h"
#include "aboutbox.h"
#include "version.h"

SerialTool::SerialTool(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
    setWindowTitle(SOFTWARE_NAME SOFTWARE_VERSION);
    listViewInit();

    // �������õĿؼ��ƶ���������
    ui.toolBar1->insertWidget(ui.portSetAction, ui.portConfigWidget);

    // ���⶯��
    tabActionGroup = new QActionGroup(this);
    tabActionGroup->addAction(ui.actionVisibleTab0);
    tabActionGroup->addAction(ui.actionVisibleTab1);

    // �϶�ʱ�������
    ui.customPlot->setNoAntialiasingOnDrag(true);
    //ui.customPlot->setNotAntialiasedElement(QCP::aePlottables);

    for (int i = 0; i < CH_NUM; ++i) {
        count[i] = 0.0;
        ui.customPlot->addGraph();
    }

    serialPort = new QSerialPort;

    ui.customPlot->axisRect()->setupFullAxesBox(true);
    ui.customPlot->setInteractions(QCP::iRangeDrag);

    scanPort(); // ɨ��˿�

    rxCount = 0;
    txCount = 0;
    // ״̬������
    rxCntLabel = new QLabel("Rx: 0Bytes", this);
    txCntLabel = new QLabel("Tx: 0Bytes", this);
    portInfoLabel = new QLabel("", this);
    rxCntLabel->setMinimumWidth(120);
    txCntLabel->setMinimumWidth(120);
    portInfoLabel->setMinimumWidth(120);
    ui.statusBar->addWidget(portInfoLabel);
    ui.statusBar->addWidget(rxCntLabel);
    ui.statusBar->addWidget(txCntLabel);
    
    // create connection between axes and scroll bars:
    connect(ui.horizontalScrollBar, SIGNAL(sliderMoved(int)), this, SLOT(horzScrollBarChanged(int)));
    connect(ui.customPlot, SIGNAL(mouseMove(QMouseEvent*)), this, SLOT(plotMouseMove()));
    connect(ui.channelList, SIGNAL(itemChanged(ChannelItem*)), this, SLOT(channelVisibleChanged(ChannelItem*)));
    connect(ui.portRunAction, SIGNAL(triggered()), this, SLOT(changeRunFlag()));
    connect(ui.openPortAction, SIGNAL(triggered()), this, SLOT(onOpenPortActionTriggered()));
    connect(serialPort, SIGNAL(errorOccurred(QSerialPort::SerialPortError)), this, SLOT(onOpenPortActionTriggered()));
    connect(serialPort, &QSerialPort::readyRead, this, &SerialTool::readPortData);
    connect(ui.sendButton, &QPushButton::clicked, this, &SerialTool::onSendButtonClicked);
    connect(ui.clearAction, SIGNAL(triggered()), this, SLOT(cleanData()));
    QObject::connect(&resendTimer, &QTimer::timeout, this, &SerialTool::writePortData);
    QObject::connect(&secTimer, &QTimer::timeout, this, &SerialTool::onSecTimerTimeout);
    connect(ui.resendBox, &QCheckBox::stateChanged, this, &SerialTool::onResendBoxChanged);
    connect(ui.comboBoxBaudRate, &QComboBox::currentTextChanged, this, &SerialTool::setPortBaudRate);
    connect(ui.spinBoxStepTime, SIGNAL(valueChanged(int)), this, SLOT(resendTimeChange(int)));
    connect(&dataTimer, SIGNAL(timeout()), this, SLOT(realtimeDataSlot()));
    connect(ui.portSetAction, SIGNAL(triggered()), this, SLOT(openSetPortInfoBox()));
    connect(ui.actionOption, SIGNAL(triggered()), this, SLOT(setOptions()));
    connect(ui.actionSave, SIGNAL(triggered()), this, SLOT(saveFile()));
    connect(ui.actionClose, SIGNAL(triggered()), this, SLOT(close()));
    connect(ui.actionVisibleToolbar, SIGNAL(triggered(bool)), ui.toolBar1, SLOT(setVisible(bool)));
    connect(ui.actionVisibleStatusBar, SIGNAL(triggered(bool)), ui.statusBar, SLOT(setVisible(bool)));
    connect(ui.toolBar1, SIGNAL(visibilityChanged(bool)), ui.actionVisibleToolbar, SLOT(setChecked(bool)));
    connect(ui.tabWidget, SIGNAL(currentChanged(int)), this, SLOT(tabIndexChanged(int)));
    connect(tabActionGroup, SIGNAL(triggered(QAction*)), this, SLOT(tabActionGroupTriggered(QAction*)));
    connect(ui.yRateLowerBox, SIGNAL(valueChanged(double)), this, SLOT(setYRateLower(double)));
    connect(ui.yRateUpperBox, SIGNAL(valueChanged(double)), this, SLOT(setYRateUpper(double)));
    connect(ui.xRangeBox, SIGNAL(currentTextChanged(const QString &)), this, SLOT(setXRange(const QString &)));
    connect(ui.actionAbout, SIGNAL(triggered()), this, SLOT(about()));
    connect(ui.comboBox, SIGNAL(currentTextChanged(const QString &)), this, SLOT(onComboBoxChanged(const QString &)));

    loadConfig(); // ��������

    dataTimer.start(20);
    secTimer.start(1000);
}

SerialTool::~SerialTool()
{
    
}

// �ر��¼�
void SerialTool::closeEvent(QCloseEvent *event)
{
    saveConfig();
}

// ��ȡ����
void SerialTool::loadConfig()
{
    config = new QSettings("config.ini", QSettings::IniFormat);

    // ���ò�����
    config->beginGroup("SerialPort");
    serialPort->setBaudRate(config->value("BaudRate").toInt());
    ui.comboBoxBaudRate->setCurrentText(QString::number(serialPort->baudRate()));
    config->endGroup();

    // ��ҳ������
    config->beginGroup("Workspace");
    ui.tabWidget->setCurrentIndex(config->value("TabIndex").toInt());
    tabIndexChanged(ui.tabWidget->currentIndex());
    ui.toolBar1->setVisible(config->value("ToolBarVisible").toBool());
    ui.actionVisibleToolbar->setChecked(ui.toolBar1->isVisible());
    ui.statusBar->setVisible(config->value("StatusBarVisible").toBool());
    // �������ֱ���ٶ�ȡui.statusBar->isVisible()����false,ԭ����
    ui.actionVisibleStatusBar->setChecked(config->value("StatusBarVisible").toBool());
    config->endGroup();

    // ���ڵ��Թ�������
    config->beginGroup("TestTool");
    if (config->value("ReceiveMode") == "Hex") {
        ui.portReadHex->setChecked(true);
    } else {
        ui.portReadAscii->setChecked(true);
    }
    if (config->value("TransmitMode") == "Hex") {
        ui.portWriteHex->setChecked(true);
    } else {
        ui.portWriteAscii->setChecked(true);
    }
    if (config->value("RepeatInterval").toInt() == 0) {
        ui.spinBoxStepTime->setValue(1000);
    } else {
        ui.spinBoxStepTime->setValue(config->value("RepeatInterval").toInt());
    }
    ui.resendBox->setChecked(config->value("ResendMode").toBool()); // �ظ�����
    onResendBoxChanged(ui.resendBox->isChecked());
    config->endGroup();

    // ����ʾ����
    config->beginGroup("Oscillograph");
    ui.yRateUpperBox->setValue(config->value("YAxisUpper").toReal());
    ui.yRateLowerBox->setValue(config->value("YAxisLower").toReal());
    for (int i = 0; i < CH_NUM; ++i) {
        ChannelItem *item = (ChannelItem *)(
            ui.channelList->itemWidget(ui.channelList->item(i)));
        item->setChecked(
            config->value("Ch" + QString::number(i + 1) + "Visible").toBool());
        item->setColor(QColor(
            config->value("Ch" + QString::number(i + 1) + "Color").toString()));
        ui.customPlot->graph(i)->setPen(QPen(item->color()));
    }
    ui.customPlot->yAxis->setRange(ui.yRateLowerBox->value(), ui.yRateUpperBox->value());
    ui.xRangeBox->setCurrentText(config->value("xAxisRange").toString());
    setXRange(ui.xRangeBox->currentText());
    config->endGroup();

    // ϵͳ����
    config->beginGroup("Settings");

    QString fonts(
        "font-family:" + config->value("FontFamily").toString().replace("+", ",") + ";\n"
        + "font:" + config->value("FontStyle").toString() + " "
        + config->value("FontSize").toString() + "px;\n");
    // ע�⣬ʹ��QTextEdit::setTextColor()�����޸�ȫ����ɫ
    ui.textEditRead->setStyleSheet(fonts + "color:" +
        config->value("ReceiveTextColor").toString());
    ui.textEditWrite->setStyleSheet(fonts + "color:" +
        config->value("TransmitTextColor").toString());
    ui.customPlot->setBackground(QBrush(QColor(
        config->value("PlotBackground").toString())));
    QColor color = QColor(config->value("AxisColor").toString());
    QPen pen(color);
    ui.customPlot->xAxis->setBasePen(pen);
    ui.customPlot->xAxis->setTickPen(pen);
    ui.customPlot->xAxis->grid()->setPen(pen);
    ui.customPlot->xAxis->setSubTickPen(pen);
    ui.customPlot->xAxis->setTickLabelColor(color);
    ui.customPlot->xAxis->setLabelColor(color);
    ui.customPlot->xAxis->grid()->setZeroLinePen(pen); // ��㻭��
    ui.customPlot->yAxis->setBasePen(pen);
    ui.customPlot->yAxis->setTickPen(pen);
    ui.customPlot->yAxis->grid()->setPen(pen);
    ui.customPlot->yAxis->setSubTickPen(pen);
    ui.customPlot->yAxis->setTickLabelColor(color);
    ui.customPlot->yAxis->setLabelColor(color);
    ui.customPlot->yAxis->grid()->setZeroLinePen(pen); // ��㻭��
    ui.customPlot->xAxis2->setBasePen(pen);
    ui.customPlot->xAxis2->setTickPen(pen);
    ui.customPlot->xAxis2->setSubTickPen(pen);
    ui.customPlot->yAxis2->setBasePen(pen);
    ui.customPlot->yAxis2->setTickPen(pen);
    ui.customPlot->yAxis2->setSubTickPen(pen);
    // OpenGl����
    if (config->value("UseOpenGL").toBool()) {
        ui.customPlot->setOpenGl(true, 16);
        if (ui.customPlot->openGl() == false) {
            config->setValue("UseOpenGL", QVariant(false));
        }
    } else {
        ui.customPlot->setOpenGl(false);
    }
    // ����ʱ���ο����, ֻ����OpenG�ر�ʱ��Ч
    if (config->value("PlotAntialiased").toBool()) {
        ui.customPlot->setNotAntialiasedElement(QCP::aePlottables, false);
    } else {
        ui.customPlot->setNotAntialiasedElement(QCP::aePlottables, true);
    }
    // ����ʱ���񿹾��, ֻ����OpenG�ر�ʱ��Ч
    if (config->value("GridAntialiased").toBool()) {
        ui.customPlot->setAntialiasedElement(QCP::aeGrid, true);
        ui.customPlot->setAntialiasedElement(QCP::aeAxes, true);
    } else {
        ui.customPlot->setAntialiasedElement(QCP::aeGrid, false);
        ui.customPlot->setAntialiasedElement(QCP::aeAxes, false);
    }
    config->endGroup();

    // ·��
    config->beginGroup("Path");
    docPath = config->value("DocumentPath").toString();
    config->endGroup();

    // �ؼ�����
    config->beginGroup("WidgetData");
    config->beginGroup("comboBox");
    int count = config->value("Count").toInt();
    for (int i = 0; i < count; ++i) {
        ui.comboBox->insertItem(0,
            config->value("Item" + QString::number(i)).toString());
    }
    config->endGroup();
    config->endGroup();
}

// ��������
void SerialTool::saveConfig()
{
    // ���沨����
    config->beginGroup("SerialPort");
    config->setValue("BaudRate",
        QVariant(ui.comboBoxBaudRate->currentText()));
    config->endGroup();
    
    // ��ҳ������
    config->beginGroup("Workspace");
    config->setValue("TabIndex",
        QVariant(QString::number(ui.tabWidget->currentIndex())));
    config->setValue("ToolBarVisible", QVariant(ui.toolBar1->isVisible()));
    config->setValue("StatusBarVisible", QVariant(ui.statusBar->isVisible()));
    config->endGroup();

    // ���ڵ��Թ�������
    config->beginGroup("TestTool");
    if (ui.portReadHex->isChecked()) {
        config->setValue("ReceiveMode", QVariant("Hex"));
    } else {
        config->setValue("ReceiveMode", QVariant("ASCII"));
    }
    if (ui.portWriteHex->isChecked()) {
        config->setValue("TransmitMode", QVariant("Hex"));
    } else {
        config->setValue("TransmitMode", QVariant("ASCII"));
    }
    config->setValue("RepeatInterval",
        QVariant(QString::number(ui.spinBoxStepTime->value())));
    config->setValue("ResendMode",
        QVariant(QString::number(ui.resendBox->isChecked())));
    config->endGroup();

    // ����ʾ����
    config->beginGroup("Oscillograph");
    config->setValue("YAxisUpper",
        QVariant(QString::number(ui.yRateUpperBox->value())));
    config->setValue("YAxisLower",
        QVariant(QString::number(ui.yRateLowerBox->value())));
    config->setValue("xAxisRange",
        QVariant(ui.xRangeBox->currentText()));
    for (int i = 0; i < CH_NUM; ++i) {
        ChannelItem *item = (ChannelItem *)(
            ui.channelList->itemWidget(ui.channelList->item(i)));
        config->setValue("Ch" + QString::number(i + 1) + "Visible",
            QVariant(item->isChecked()));
        config->setValue("Ch" + QString::number(i + 1) + "Color",
                QVariant(item->color().name()));
    }
    config->endGroup();

    // ·��
    config->beginGroup("Path");
    config->setValue("DocumentPath", QVariant(docPath));
    config->endGroup();

    // �ؼ�����
    config->beginGroup("WidgetData");
    config->beginGroup("comboBox");
    int count = ui.comboBox->count();
    config->setValue("Count", count);
    for (int i = 0; i < count; ++i) {
        config->setValue("Item" + QString::number(i),
            QVariant(ui.comboBox->itemText(i)));
    }
    config->endGroup();
    config->endGroup();
}

void SerialTool::setOptions()
{
    OptionsBox option(this);
    option.exec();
}

// �����ļ�
void SerialTool::saveFile()
{
    QString filter;
    QString fname = QFileDialog::getSaveFileName(this, "Save", docPath,
        "*.png;;*.bmp;;*.jpg;;*.pdf", &filter,
        QFileDialog::HideNameFilterDetails);
    if (fname.isNull()) {
        return;
    }
    docPath = QFileInfo(fname).path();
    if (filter == "*.png") {
        ui.customPlot->savePng(fname);
    } else if (filter == "*.bmp") {
        ui.customPlot->saveBmp(fname);
    } else if (filter == "*.jpg") {
        ui.customPlot->saveJpg(fname);
    } else if (filter == "*.pdf") {
        ui.customPlot->savePdf(fname);
    }
}

// �����ƶ�����
void SerialTool::horzScrollBarChanged(int value)
{
    if (ui.horizontalScrollBar->maximum() == value) {
        replotFlag = true;
    } else {
        replotFlag = false;
    }
    ui.customPlot->xAxis->setRange(value / (100.0 / xRange) + xRange,
        ui.customPlot->xAxis->range().size(), Qt::AlignRight);
    ui.customPlot->replot();
}

// ����϶�
void SerialTool::plotMouseMove()
{
    double upper = ui.customPlot->xAxis->range().upper;
    int key = (int)((upper - xRange) * (100.0 / xRange));
    if (upper > xRange) {
        if (ui.horizontalScrollBar->maximum() == key) {
            replotFlag = true;
        } else {
            replotFlag = false;
        }
        ui.horizontalScrollBar->setValue(key);
    }
    ui.yRateUpperBox->setValue(ui.customPlot->yAxis->range().upper);
    ui.yRateLowerBox->setValue(ui.customPlot->yAxis->range().lower);
}


void SerialTool::setYRateLower(double d)
{
    ui.customPlot->yAxis->setRangeLower(d);
    ui.yRateUpperBox->setValue(ui.customPlot->yAxis->range().upper);
}

void SerialTool::setYRateUpper(double d)
{
    ui.customPlot->yAxis->setRangeUpper(d);
    ui.yRateLowerBox->setValue(ui.customPlot->yAxis->range().lower);
}

void SerialTool::setXRange(const QString &str)
{
    double upper = ui.customPlot->xAxis->range().upper;
    xRange = str.toDouble();
    if (upper < xRange) {
        ui.horizontalScrollBar->setRange(0, 0);
        ui.horizontalScrollBar->setValue(0);
        ui.customPlot->xAxis->setRange(0, xRange);
    } else {
        ui.horizontalScrollBar->setRange(0, (int)((upper - xRange) * (100.0 / xRange)));
        ui.horizontalScrollBar->setValue((int)((upper - xRange) * (100.0 / xRange)));
        ui.customPlot->xAxis->setRange(upper, xRange, Qt::AlignRight);
    }
    ui.customPlot->replot();
}

void SerialTool::tabIndexChanged(int index)
{
    if (index == 0) {
        ui.actionVisibleTab0->setChecked(true);
    } else {
        ui.actionVisibleTab1->setChecked(true);
    }
}

void SerialTool::tabActionGroupTriggered(QAction *action)
{
    if (action == ui.actionVisibleTab0) {
        ui.tabWidget->setCurrentIndex(0);
    } else {
        ui.tabWidget->setCurrentIndex(1);
    }
    
}

// �������
void SerialTool::addData(int channel, double key, double value)
{
    ui.customPlot->graph(channel)->addData(key, value);
}

void SerialTool::setChannelVisible(int ch, bool on)
{
    ui.customPlot->graph(ch)->setVisible(on);
}

void SerialTool::channelStyleChanged(ChannelItem *item)
{
    int ch = item->channel();
    if (item->isChecked()) {
        setChannelVisible(ch, true);
    } else {
        setChannelVisible(ch, false);
    }
    ui.customPlot->graph(ch)->setPen(QPen(item->color()));
    ui.customPlot->replot();
}

void SerialTool::realtimeDataSlot()
{
    // ��ʾ����
    if (runFlag) {
        double key = count[0];
        for (int i = 0; i < CH_NUM; ++i) {
            key = key > count[i] ? key : count[i];
        }
        Qt::AlignmentFlag align;
        if (key > xRange) {
            ui.horizontalScrollBar->setRange(0, (int)((key - xRange) * (100.0 / xRange)));
            if (replotFlag) {
                ui.horizontalScrollBar->setValue((int)((key - xRange) * (100.0 / xRange)));
                ui.customPlot->xAxis->setRange(key, xRange, Qt::AlignRight);
            }
        }
        ui.customPlot->replot();
    }
}

void SerialTool::changeRunFlag()
{
    if (runFlag == true) {
        runFlag = false;
        QIcon icon(":/SerialTool/images/start.ico");
        ui.portRunAction->setIcon(icon);
        ui.portRunAction->setText(QStringLiteral("��������/����"));
    } else {
        runFlag = true;
        QIcon icon(":/SerialTool/images/pause.ico");
        ui.portRunAction->setIcon(icon);
        ui.portRunAction->setText(QStringLiteral("��ͣ����/����"));
    }
}

void SerialTool::onSecTimerTimeout()
{
    const QString parity[] {
        "None", "Even", "Odd", "Space", "Mark", "Unknown"
    };
    const QString flowControl[] {
        "None", "RTS/CTS", "XON/XOFF", "Unknown"
    };

    if (!serialPort->isOpen()) { // ���ڹر�ʱɨ����ö˿�
        scanPort();
    }
    // ������ʾ��Ϣ
    QString str;
    str = ui.comboBoxPortNum->currentText().section(" ", 0, 0) + " ";
    if (serialPort->isOpen()) {
        str += "OPEND, " + QString::number(serialPort->baudRate()) + "bps, "
            + QString::number(serialPort->dataBits()) + "bit, "
            + parity[serialPort->parity()] + ", "
            + QString::number(serialPort->stopBits()) + ", "
            + flowControl[serialPort->flowControl()];
        portInfoLabel->setStyleSheet("color:green");
    } else {
        str += "CLOSED";
        portInfoLabel->setStyleSheet("color:red");
    }
    portInfoLabel->setText(str);
    str = "Rx: " + QString::number(rxCount) + "Bytes";
    rxCntLabel->setText(str);
    str = "Tx: " + QString::number(txCount) + "Bytes";
    txCntLabel->setText(str);
}

// ɨ��˿�
void SerialTool::scanPort()
{
    int i = 0;
    bool sync = false;
    QVector<QSerialPortInfo> vec;
    //���ҿ��õĴ���
    foreach(const QSerialPortInfo &info, QSerialPortInfo::availablePorts())
    {
        // �˿�������ƥ��˵���˿��б���
        if (ui.comboBoxPortNum->itemText(i++).indexOf(info.portName())) {
            sync = true;
        }
        vec.append(info);
    }
    // ��Ҫͬ������comboBoxPortNum������Ч�˿�
    if (sync || !ui.comboBoxPortNum->itemText(i).isEmpty()) {
        QString str = ui.comboBoxPortNum->currentText();
        ui.comboBoxPortNum->clear();
        for (i = 0; i < vec.length(); ++i) {
            ui.comboBoxPortNum->addItem(vec[i].portName()
                + " (" + vec[i].description() + ")");
        }
        if (!str.isEmpty()) {
            ui.comboBoxPortNum->setCurrentText(str);
        } else {
            ui.comboBoxPortNum->setCurrentIndex(0);
        }
    }
}

// �򿪴��ڲۺ���
void SerialTool::onOpenPortActionTriggered()
{ 
    if (serialPort->isOpen() == true) {
        serialPort->close(); // �رմ���
        QIcon icon(":/SerialTool/images/connect.png");
        ui.openPortAction->setIcon(icon);
        ui.openPortAction->setText(QStringLiteral("�򿪶˿�"));
        ui.comboBoxPortNum->setEnabled(true); // ������Ĵ���
        ui.sendButton->setEnabled(false);
        ui.portRunAction->setEnabled(false);
    } else {
        QString name = ui.comboBoxPortNum->currentText().section(' ', 0, 0);
        serialPort->setPortName(name);
        if (serialPort->open(QIODevice::ReadWrite)) {
            QIcon icon(":/SerialTool/images/close.png");
            ui.openPortAction->setIcon(icon);
            ui.openPortAction->setText(QStringLiteral("�رն˿�"));
            ui.comboBoxPortNum->setEnabled(false); // ��ֹ���Ĵ���
            ui.sendButton->setEnabled(true);
            ui.portRunAction->setEnabled(true);
        } else {
            QMessageBox err(QMessageBox::Critical, 
                QStringLiteral("Error"),
                QStringLiteral("�޷��򿪶˿ڣ�\n�˿ڿ��ܱ�ռ�û������ô���"),
                QMessageBox::Cancel, this);
            err.exec();
        }
    }
}

void SerialTool::openSetPortInfoBox()
{
    PortSetBox port(serialPort);
    port.exec();
}

void SerialTool::setPortBaudRate(const QString &string)
{
    serialPort->setBaudRate(string.toInt());
}

void SerialTool::onSendButtonClicked()
{
    if (!runFlag) { // ����û����·��Ͱ�ť���˳���ͣģʽ
        changeRunFlag();
    }
    writePortData();

    // �����б�ɾ��������
    while (ui.comboBox->count() >= 20) {
        ui.comboBox->removeItem(19);
    }
    // ����д����ʷ��¼�����б�
    QString str = ui.textEditWrite->toPlainText();
    int i = ui.comboBox->findText(str);
    if (i != -1) { // ���ڵ�����ɾ��
        ui.comboBox->removeItem(i);
    }
    ui.comboBox->insertItem(0, str); // ������ӵ���0��Ԫ��
    ui.comboBox->setCurrentIndex(0);
}

// ����֡����:
// byte[0]: 'C'
// byte[1]: 'H'
// byte[2]: ͨ��
// byte[3]: float[0]
// byte[4]: float[1]
// byte[5]: float[2]
// byte[6]: float[3]
static bool serialPortGetByte(char &ch, float &value, char byte)
{
    static quint8 buffer[5], pos = 0, last = '\0';
    
    // ����֡ͷ״̬��
    switch (last) {
    case 0:
        last = byte == 'C' ? 'C' : 0;
        pos = 0;
        break;
    case 'C':
        last = byte == 'H' ? 'H' : 0;
        break;
    case 'H':
        last = (quint8)byte < CH_NUM  ? 'T' : 0;
    case 'T':
        buffer[pos++] = byte;
        if (pos == 5) {
            last = 0;
            union {
                quint32 i;
                float f;
            } data;
            data.i = (quint32)buffer[1] << 24;
            data.i |= (quint32)buffer[2] << 16;
            data.i |= (quint32)buffer[3] << 8;
            data.i |= (quint32)buffer[4];
            value = data.f;
            ch = buffer[0];
            return true;
        }
        break;
    default:
        last = 0;
    }
    return false;
}

static void byteArrayToHex(QString &str, QByteArray &arr)
{
    unsigned int len = arr.length();
    str.resize(len * 3);
    for (int i = 0; i < len; ++i) {
        int j;
        quint8 outChar = arr[i], t;   //ÿ�ֽ����һ�Σ�ֱ������
                                      //ʮ�����Ƶ�ת��
        j = i * 3;
        t = (outChar >> 4);
        str[j] = t + (t < 10 ? '0' : 'A' - 10);
        t = outChar & 0x0F;
        str[j + 1] = t + (t < 10 ? '0' : 'A' - 10);
        str[j + 2] = ' ';
    }
}

//��ȡ���յ�������  
void SerialTool::readPortData()
{
    // ��ȡ��������
    if (!runFlag) {
        return;
    }
    QByteArray buf = serialPort->readAll(); // ��ȡ��������
    rxCount += buf.length(); // ���ռ���
    if (!buf.isEmpty()) {
        if (ui.tabWidget->currentIndex() == 0) { // ���ڵ�������
            QString str;
            if (ui.portReadAscii->isChecked()) {
                str = QString::fromLocal8Bit(buf);
            } else {
                byteArrayToHex(str, buf);
            }
            // ����Ĵ�������ʵ����QTextEdit::append()�Ĺ���
            // ӦΪQTextEdit::append()���ǻ��Դ�����
            // ����ο��� http://stackoverflow.com/questions/13559990/how-to-append-text-to-qplaintextedit-without-adding-newline-and-keep-scroll-at
            QScrollBar *pScrolBar = ui.textEditRead->verticalScrollBar();
            bool bool_at_bottom = (pScrolBar->value() == pScrolBar->maximum());
            QTextCursor text_cursor = ui.textEditRead->textCursor();
            text_cursor.movePosition(QTextCursor::End);
            text_cursor.insertText(str);
            if (bool_at_bottom) {
                pScrolBar->setValue(pScrolBar->maximum());
            }
        }
        // ����ʾ������������
        if (ui.tabWidget->currentIndex() == 1 || ui.holdRxOscBox->isChecked()) {
            for (int i = 0; i < buf.length(); ++i) {
                char ch;
                float value;
                if (serialPortGetByte(ch, value, buf.data()[i]) == true) {
                    addData(ch, count[ch], value);
                    count[ch] += 1.0; // ����
                }
            }
        }
    }
    buf.clear();
}

// �򴮿ڷ�������
void SerialTool::writePortData()
{
    if (runFlag) {
        QByteArray arr;
        if (ui.portWriteAscii->isChecked() == true) {
            QTextCodec *code = QTextCodec::codecForName("GB-2312");
            arr = code->fromUnicode(ui.textEditWrite->toPlainText());
        } else {
            arr = QByteArray::fromHex(
                ui.textEditWrite->toPlainText().toLatin1());
        }
        txCount += arr.length(); // ���ͼ���
        serialPort->write(arr.data(), arr.length());
    }
}

void SerialTool::cleanData()
{
    switch (ui.tabWidget->currentIndex()) {
    case 0: // ���ڵ�������
        ui.textEditRead->clear();
        break;
    case 1: // ����ʾ����
        for (int i = 0; i < CH_NUM; ++i) {
            ui.customPlot->graph(i)->data()->clear();
            ui.horizontalScrollBar->setValue(0);
            ui.horizontalScrollBar->setRange(0, 0);
            ui.customPlot->xAxis->setRange(0, xRange, Qt::AlignLeft);
            ui.customPlot->replot();
            count[i] = 0.0;
        }
        break;
    }
    // ��������
    rxCount = 0;
    txCount = 0;
}

void SerialTool::onResendBoxChanged(int status)
{
    if (status) {
        resendTimer.start(ui.spinBoxStepTime->text().toInt());
    } else {
        resendTimer.stop();
    }
}

//  �ظ�����ʱ���޸�
void SerialTool::resendTimeChange(int msc)
{
    resendTimer.setInterval(msc);
}

void SerialTool::listViewInit()
{
    ui.channelList->setModelColumn(2); // ����
    for (int i = 0; i < CH_NUM; ++i) {
        QListWidgetItem *item = new QListWidgetItem;
        ui.channelList->addItem(item);
        ChannelItem *chItem = new ChannelItem("CH" + QString::number(i + 1));
        ui.channelList->setItemWidget(item, chItem);
        chItem->setChannel(i);
        connect(chItem, &ChannelItem::changelChanged, this, &SerialTool::channelStyleChanged);
    }
}

void SerialTool::about()
{
    AboutBox aboutBox(this);
    
    aboutBox.exec();
}

void SerialTool::onComboBoxChanged(const QString &string)
{
    ui.textEditWrite->setText(string);
    ui.textEditWrite->moveCursor(QTextCursor::End);
}
