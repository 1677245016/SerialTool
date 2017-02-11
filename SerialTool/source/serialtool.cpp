﻿#include "serialtool.h"
#include "portsetbox.h"
#include "optionsbox.h"
#include "aboutbox.h"
#include "version.h"

SerialTool::SerialTool(QWidget *parent)
    : QMainWindow(parent)
{
    config = new QSettings("config.ini", QSettings::IniFormat);

    ui.setupUi(this);
    setWindowTitle(SOFTWARE_NAME " V" SOFTWARE_VERSION);
    listViewInit();

    // 串口设置的控件移动到工具栏
    ui.toolBar1->insertWidget(ui.portSetAction, ui.portConfigWidget);

    // 互斥动作
    tabActionGroup = new QActionGroup(this);
    tabActionGroup->addAction(ui.actionVisibleTab0);
    tabActionGroup->addAction(ui.actionVisibleTab1);

    // 对界面的初始化借宿之后再设置语言和样式表
    qApp->installTranslator(&appTranslator);
    qApp->installTranslator(&qtTranslator);
    setLanguage("zh_CN");
    setStyleSheet("default");

    // 拖动时不抗锯齿
    ui.customPlot->setNoAntialiasingOnDrag(true);

    for (int i = 0; i < CH_NUM; ++i) {
        count[i] = 0.0;
        ui.customPlot->addGraph();
    }

    serialPort = new QSerialPort;

    ui.customPlot->axisRect()->setupFullAxesBox(true);
    ui.customPlot->setInteractions(QCP::iRangeDrag);

    scanPort(); // 扫描端口

    rxCount = 0;
    txCount = 0;
    // 状态栏设置
    rxCntLabel = new QLabel("Rx: 0Bytes", this);
    txCntLabel = new QLabel("Tx: 0Bytes", this);
    portInfoLabel = new QLabel("", this);
    rxCntLabel->setMinimumWidth(120);
    txCntLabel->setMinimumWidth(120);
    portInfoLabel->setMinimumWidth(120);
    ui.statusBar->addWidget(portInfoLabel);
    ui.statusBar->addWidget(rxCntLabel);
    ui.statusBar->addWidget(txCntLabel);

    ui.textEditRx->setReadOnly(true);

    // 发送区自动换行
    ui.textEditTx->setWrap(1);

    loadConfig(); // 加载配置
    
    // create connection between axes and scroll bars:
    connect(ui.horizontalScrollBar, SIGNAL(sliderMoved(int)), this, SLOT(horzScrollBarMoved(int)));
    connect(ui.horizontalScrollBar, SIGNAL(actionTriggered(int)), this, SLOT(horzScrollBarTriggered()));
    connect(ui.customPlot, SIGNAL(mouseMove(QMouseEvent*)), this, SLOT(plotMouseMove()));
    connect(ui.portRunAction, SIGNAL(triggered()), this, SLOT(changeRunFlag()));
    connect(ui.portSwitchAction, SIGNAL(triggered()), this, SLOT(onPortSwitchActionTriggered()));
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
    connect(ui.comboBox, SIGNAL(activated(const QString &)), this, SLOT(onComboBoxChanged(const QString &)));
    connect(ui.wrapLineBox, SIGNAL(stateChanged(int)), this, SLOT(onWrapBoxChanged(int)));
    
    dataTimer.start(20);
    secTimer.start(1000);
}

SerialTool::~SerialTool()
{
    
}

// 关闭事件
void SerialTool::closeEvent(QCloseEvent *event)
{
    saveConfig();
}

// 加载语言
void SerialTool::setLanguage(const QString &string)
{
    appTranslator.load("language/" + string + ".qm");
    qtTranslator.load("language/qt_" + string + ".qm");
    ui.retranslateUi(this);
}

// 加载样式表
void SerialTool::setStyleSheet(const QString &string)
{
    // 样式表
    QFile qss("themes/" + string + ".css");
    qss.open(QFile::ReadOnly);
    qApp->setStyleSheet(qss.readAll());
    qss.close();
}

// 读取系统设置
void SerialTool::loadSettings()
{
    // 系统设置
    config->beginGroup("Settings");

    QString fonts("'" 
        + config->value("FontFamily").toString().replace("+", "','") + "'");
    QString fontStyle(config->value("FontStyle").toString());
    int fontSize = config->value("FontSize").toInt();
    ui.textEditRx->setFonts(fonts, fontSize,
        QColor(config->value("ReceiveTextColor").toString()), fontStyle);
    ui.textEditTx->setFonts(fonts, fontSize,
        QColor(config->value("ReceiveTextColor").toString()), fontStyle);

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
    ui.customPlot->xAxis->grid()->setZeroLinePen(pen); // 零点画笔
    ui.customPlot->yAxis->setBasePen(pen);
    ui.customPlot->yAxis->setTickPen(pen);
    ui.customPlot->yAxis->grid()->setPen(pen);
    ui.customPlot->yAxis->setSubTickPen(pen);
    ui.customPlot->yAxis->setTickLabelColor(color);
    ui.customPlot->yAxis->setLabelColor(color);
    ui.customPlot->yAxis->grid()->setZeroLinePen(pen); // 零点画笔
    ui.customPlot->xAxis2->setBasePen(pen);
    ui.customPlot->xAxis2->setTickPen(pen);
    ui.customPlot->xAxis2->setSubTickPen(pen);
    ui.customPlot->yAxis2->setBasePen(pen);
    ui.customPlot->yAxis2->setTickPen(pen);
    ui.customPlot->yAxis2->setSubTickPen(pen);

    // 绘制时波形抗锯齿
    if (config->value("PlotAntialiased").toBool()) {
        ui.customPlot->setNotAntialiasedElement(QCP::aePlottables, false);
    } else {
        ui.customPlot->setNotAntialiasedElement(QCP::aePlottables, true);
    }
    // 绘制时网格抗锯齿
    if (config->value("GridAntialiased").toBool()) {
        ui.customPlot->setAntialiasedElement(QCP::aeGrid, true);
        ui.customPlot->setAntialiasedElement(QCP::aeAxes, true);
    } else {
        ui.customPlot->setAntialiasedElement(QCP::aeGrid, false);
        ui.customPlot->setAntialiasedElement(QCP::aeAxes, false);
    }
    config->endGroup();
}

// 控件数据初始化, 在构造函数中初始化各种控件的初始值
void SerialTool::loadConfig()
{
    // 路经
    config->beginGroup("Path");
    docPath = config->value("DocumentPath").toString();
    config->endGroup();

    // 控件数据
    config->beginGroup("WidgetData");
    config->beginGroup("comboBox");
    int count = config->value("Count").toInt();
    for (int i = 0; i < count; ++i) {
        ui.comboBox->addItem(
            config->value("Item" + QString::number(i)).toString());
    }
    ui.comboBox->setCurrentIndex(0);
    config->endGroup();
    config->endGroup();

    // 配置波特率
    config->beginGroup("SerialPort");
    serialPort->setBaudRate(config->value("BaudRate").toInt());
    ui.comboBoxBaudRate->setCurrentText(QString::number(serialPort->baudRate()));
    config->endGroup();

    // 打开页面配置
    config->beginGroup("Workspace");
    ui.tabWidget->setCurrentIndex(config->value("TabIndex").toInt());
    tabIndexChanged(ui.tabWidget->currentIndex());
    ui.toolBar1->setVisible(config->value("ToolBarVisible").toBool());
    ui.actionVisibleToolbar->setChecked(ui.toolBar1->isVisible());
    ui.statusBar->setVisible(config->value("StatusBarVisible").toBool());
    // 这里如果直接速读取ui.statusBar->isVisible()会是false,原因不明
    ui.actionVisibleStatusBar->setChecked(config->value("StatusBarVisible").toBool());
    config->endGroup();

    // 串口调试工具配置
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
    if (config->value("RepeatInterval").toInt() == 0) { // 自动重发时间
        ui.spinBoxStepTime->setValue(1000);
    } else {
        ui.spinBoxStepTime->setValue(config->value("RepeatInterval").toInt());
    }
    ui.resendBox->setChecked(config->value("ResendMode").toBool()); // 重复发送
    onResendBoxChanged(ui.resendBox->isChecked());
    ui.wrapLineBox->setChecked(config->value("RxAreaWrapLine").toBool()); // 接收区自动换行
    onWrapBoxChanged(ui.wrapLineBox->isChecked());
    config->endGroup();

    // 串口示波器
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

    // 最后读取系统设置
    loadSettings();
}

// 保存配置
void SerialTool::saveConfig()
{
    // 保存波特率
    config->beginGroup("SerialPort");
    config->setValue("BaudRate",
        QVariant(ui.comboBoxBaudRate->currentText()));
    config->endGroup();
    
    // 打开页面配置
    config->beginGroup("Workspace");
    config->setValue("TabIndex",
        QVariant(QString::number(ui.tabWidget->currentIndex())));
    config->setValue("ToolBarVisible", QVariant(ui.toolBar1->isVisible()));
    config->setValue("StatusBarVisible", QVariant(ui.statusBar->isVisible()));
    config->endGroup();

    // 串口调试工具配置
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
        QVariant(ui.resendBox->isChecked())); // 自动重发
    config->setValue("RxAreaWrapLine",
        QVariant(ui.wrapLineBox->isChecked())); // 接收区自动换行
    config->endGroup();

    // 串口示波器
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

    // 路经
    config->beginGroup("Path");
    config->setValue("DocumentPath", QVariant(docPath));
    config->endGroup();

    // 控件数据
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

// 保存文件
void SerialTool::saveFile()
{
    QString filter;
    QString fname = QFileDialog::getSaveFileName(this, "Save", docPath,
        tr("Portable Network Graphic Format (*.png);;"
            "Bitmap (*.bmp);;"
           "Portable Document Format (*.pdf)"), &filter,
        QFileDialog::HideNameFilterDetails);
    if (fname.isNull()) {
        return;
    }
    docPath = QFileInfo(fname).path();
    if (filter.indexOf("(*.png)", 0)) {
        ui.customPlot->savePng(fname);
    } else if (filter.indexOf("(*.bmp)", 0)) {
        ui.customPlot->saveBmp(fname);
    } else if (filter.indexOf("(*.pdf)", 0)) {
        ui.customPlot->savePdf(fname);
    }
}

// 滚动条滑块移动触发
void SerialTool::horzScrollBarMoved(int value)
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

// 滚动条按键触发
void SerialTool::horzScrollBarTriggered()
{
    int value = ui.horizontalScrollBar->value();
    ui.customPlot->xAxis->setRange(value / (100.0 / xRange) + xRange,
        ui.customPlot->xAxis->range().size(), Qt::AlignRight);
    ui.customPlot->replot();
}

// 鼠标拖动
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

// 添加数据
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
    // 显示更新
    if (runFlag) {
        double key = count[0];
        for (int i = 0; i < CH_NUM; ++i) {
            key = key > count[i] ? key : count[i];
        }
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
        ui.portRunAction->setText(tr("Start Tx/Rx"));
    } else {
        runFlag = true;
        QIcon icon(":/SerialTool/images/pause.ico");
        ui.portRunAction->setIcon(icon);
        ui.portRunAction->setText(tr("Pause Tx/Rx"));
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

    if (serialPort->isOpen()) {
        // 检查端口是否有错误
        if (serialPort->error() != QSerialPort::NoError) {
            closePort();
            scanPort();
        }
    } else {
        scanPort(); // 串口关闭时扫描可用端口
    }
    // 更新显示信息
    QString str;
    str = ui.comboBoxPortNum->currentText().section(" ", 0, 0) + " ";
    if (serialPort->isOpen()) {
        str += "OPEND, " + QString::number(serialPort->baudRate()) + "bps, "
            + QString::number(serialPort->dataBits()) + "bit, "
            + parity[serialPort->parity()] + ", "
            + QString::number(serialPort->stopBits()) + ", "
            + flowControl[serialPort->flowControl()];
        portInfoLabel->setStyleSheet("color:green; font-family: Microsoft YaHei UI;");
    } else {
        str += "CLOSED";
        portInfoLabel->setStyleSheet("color:red; font-family: Microsoft YaHei UI;");
    }
    portInfoLabel->setText(str);
    str = "Rx: " + QString::number(rxCount) + "Bytes";
    rxCntLabel->setText(str);
    str = "Tx: " + QString::number(txCount) + "Bytes";
    txCntLabel->setText(str);
}

// 扫描端口
void SerialTool::scanPort()
{
    int i = 0;
    bool sync = false;
    QVector<QSerialPortInfo> vec;
    //查找可用的串口
    foreach(const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
        // 端口名不在匹配说明端口列表变更
        if (ui.comboBoxPortNum->itemText(i++).indexOf(info.portName())) {
            sync = true;
        }
        vec.append(info);
    }
    // 需要同步或者comboBoxPortNum存在无效端口
    if (sync || !ui.comboBoxPortNum->itemText(i).isEmpty()) {
        int len = 0;
        QString longStr, str = ui.comboBoxPortNum->currentText();
        ui.comboBoxPortNum->clear();
        for (i = 0; i < vec.length(); ++i) {
            QString t = vec[i].portName() + " (" + vec[i].description() + ")";
            ui.comboBoxPortNum->addItem(t);
            if (t.length() > len) { // 统计最长字符串
                len = t.length();
                longStr = t;
            }
        }
        if (!str.isEmpty()) {
            ui.comboBoxPortNum->setCurrentText(str);
        } else {
            ui.comboBoxPortNum->setCurrentIndex(0);
        }
        // 自动控制下拉列表宽度
        QFontMetrics fm(ui.comboBoxPortNum->font());
        len = fm.boundingRect(longStr).width() + 9;
        ui.comboBoxPortNum->view()->setMinimumWidth(len);
    }
}

void SerialTool::openPort()
{
    QString name = ui.comboBoxPortNum->currentText().section(' ', 0, 0);
    serialPort->setPortName(name);
    if (serialPort->open(QIODevice::ReadWrite)) {
        QIcon icon(":/SerialTool/images/close.png");
        ui.portSwitchAction->setIcon(icon);
        ui.portSwitchAction->setText(tr("Close Port"));
        ui.comboBoxPortNum->setEnabled(false); // 禁止更改串口
        ui.sendButton->setEnabled(true);
        ui.portRunAction->setEnabled(true);
    } else {
        QMessageBox err(QMessageBox::Critical,
            tr("Error"),
            tr("Can not open the port!\n"
                "Port may be occupied or configured incorrectly!"),
            QMessageBox::Cancel, this);
        err.exec();
    }
}

void SerialTool::closePort()
{
    serialPort->close(); // 关闭串口
    QIcon icon(":/SerialTool/images/connect.png");
    ui.portSwitchAction->setIcon(icon);
    ui.portSwitchAction->setText(tr("Open Port"));
    ui.comboBoxPortNum->setEnabled(true); // 允许更改串口
    ui.sendButton->setEnabled(false);
    ui.portRunAction->setEnabled(false);
}

// 打开串口槽函数
void SerialTool::onPortSwitchActionTriggered()
{ 
    if (serialPort->isOpen() == true) { // 现在需要关闭端口
        closePort();
    } else { // 端口关闭时打开端口
        openPort();
    }
}

// 打开串口设置对话框
void SerialTool::openSetPortInfoBox()
{
    PortSetBox portSet(serialPort);
    portSet.exec();
}

void SerialTool::setPortBaudRate(const QString &string)
{
    serialPort->setBaudRate(string.toInt());
}

void SerialTool::onSendButtonClicked()
{
    if (!runFlag) { // 如果用户按下发送按钮则退出暂停模式
        changeRunFlag();
    }
    QString str = ui.textEditTx->text();
    if (!str.isEmpty()) {
        writePortData();

        // 历史记录下拉列表删除多余项
        while (ui.comboBox->count() >= 20) {
            ui.comboBox->removeItem(19);
        }
        // 数据写入历史记录下拉列表
        int i = ui.comboBox->findText(str);
        if (i != -1) { // 存在的项先删除
            ui.comboBox->removeItem(i);
        }
        ui.comboBox->insertItem(0, str); // 数据添加到第0个元素
        ui.comboBox->setCurrentIndex(0);
    }
}

// 数据帧类容:
// byte[0]: 'C'
// byte[1]: 'H'
// byte[2]: 通道
// byte[3]: float[0]
// byte[4]: float[1]
// byte[5]: float[2]
// byte[6]: float[3]
static bool serialPortGetByte(char &ch, float &value, char byte)
{
    static quint8 buffer[5], pos = 0, last = '\0';
    
    // 捕获帧头状态机
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

static void byteArrayToHex(QString &str, QByteArray &arr, int countOfLine)
{
    static int count;
    int len = arr.length();
    str.resize(len * 3 + (len + count) / countOfLine);
    for (int i = 0, j = 0; i < len; ++i) {
        quint8 outChar = arr[i], t;   //每字节填充一次，直到结束
        //十六进制的转换
        t = (outChar >> 4);
        str[j++] = t + (t < 10 ? '0' : 'A' - 10);
        t = outChar & 0x0F;
        str[j++] = t + (t < 10 ? '0' : 'A' - 10);
        str[j++] = ' ';
        if (count >= countOfLine - 1) {
            count = 0;
            str[j++] = '\n';
        } else {
            ++count;
        }
    }
}

// 这个函数可以避免中文接收的乱码
static void byteArrayToAscii(QString &str, QByteArray &arr, QByteArray &buf)
{
    int cnt = 0;

    buf += arr;
    for (int i = buf.length() - 1; i >= 0 && (quint8)(buf[i]) > 128; --i) {
        cnt++;
    }
    if (cnt & 1) { // 字符串最末尾的非ASCII字节数不为2的整数倍
        char ch = buf[buf.length() - 1];
        buf.remove(buf.length() - 1, 1);
        str = QString::fromLocal8Bit(buf);
        buf.clear();
        buf.append(ch);
    } else {
        str = QString::fromLocal8Bit(buf);
        buf.clear();
    }
}

//读取接收到的数据  
void SerialTool::readPortData()
{
    // 读取串口数据
    if (!runFlag) {
        return;
    }
    QByteArray buf = serialPort->readAll(); // 读取串口数据
    rxCount += buf.length(); // 接收计数
    if (!buf.isEmpty()) {
        if (ui.tabWidget->currentIndex() == 0) { // 串口调试助手
            QString str;
            if (ui.portReadAscii->isChecked()) { // ASCII模式
                byteArrayToAscii(str, buf, asciiBuf);
            } else {
                byteArrayToHex(str, buf, 16);
            }
            ui.textEditRx->append(str);
        }
        // 串口示波器接收数据
        if (ui.tabWidget->currentIndex() == 1 || ui.holdRxOscBox->isChecked()) {
            for (int i = 0; i < buf.length(); ++i) {
                char ch;
                float value;
                if (serialPortGetByte(ch, value, buf.data()[i]) == true) {
                    addData(ch, count[(int)ch], value);
                    count[(int)ch] += 1.0; // 计数
                }
            }
        }
    }
    buf.clear();
}

// 向串口发送数据
void SerialTool::writePortData()
{
    if (runFlag && serialPort->isOpen()) {
        QByteArray arr;
        if (ui.portWriteAscii->isChecked() == true) {
            QTextCodec *code = QTextCodec::codecForName("GB-2312");
            arr = code->fromUnicode(ui.textEditTx->text());
        } else {
            arr = QByteArray::fromHex(ui.textEditTx->text().toLatin1());
        }
        txCount += arr.length(); // 发送计数
        serialPort->write(arr.data(), arr.length());
    }
}

void SerialTool::cleanData()
{
    switch (ui.tabWidget->currentIndex()) {
    case 0: // 串口调试助手
        ui.textEditRx->clear();
        asciiBuf.clear();
        break;
    case 1: // 串口示波器
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
    // 计数清零
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

//  重复发送时间修改
void SerialTool::resendTimeChange(int msc)
{
    resendTimer.setInterval(msc);
}

void SerialTool::listViewInit()
{
    ui.channelList->setModelColumn(2); // 两列
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
    ui.textEditTx->setText(string);
}

void SerialTool::onWrapBoxChanged(int status)
{
    ui.textEditRx->setWrap(status);
}
