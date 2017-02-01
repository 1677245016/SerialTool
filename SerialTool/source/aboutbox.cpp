#include "aboutbox.h"
#include "version.h"

AboutBox::AboutBox(QWidget *parent)
{
    // ����ʾ�ʺ�
    Qt::WindowFlags flags = Qt::Dialog;
    flags |= Qt::WindowCloseButtonHint;
    setWindowFlags(flags);
    ui.setupUi(this);
    setFixedSize(400, 400); // ���������ĶԻ���

    QPalette pal(palette());
    pal.setColor(QPalette::Background, Qt::white); //���ñ���ɫ
    setAutoFillBackground(true);
    setPalette(pal);

    ui.buttonBox->setCenterButtons(true); // ��ť����
    ui.textBrowser->setFrameStyle(QFrame::NoFrame); // �ޱ߿�

    QPixmap pix(":/SerialTool/images/icon.ico");
    pix = pix.scaledToWidth(64, Qt::SmoothTransformation);
    ui.label1->setPixmap(pix);
    setWindowTitle(QStringLiteral("����SerialTool"));
    ui.label2->setText(
        "<b>SerialTool</b><br>"
        "Version: " SOFTWARE_VERSION "." BUILD_VERSION "<br>"
        "Email: 2269610337@qq.com<br>"
        "Home Page: <a href=\"https://github.com/Le-Seul/SerialTool\">View Code On GitHub</a><br>"
        "Build Date: " __DATE__ "<br>" // ����ʱ��
        COPYRIGHT
    );
    ui.label2->setOpenExternalLinks(true); // �����������
    ui.textBrowser->setText(
        "This program is free software; you can redistribute it and / or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.\n"
        "This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the GNU General Public License for more details.\n"
        "You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.\n"
    );
}
