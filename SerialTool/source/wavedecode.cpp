#include "wavedecode.h"

enum {
    Frame_Head = 0xA3,      //֡ͷʶ����
    Frame_PointMode = 0xA8, // ��ģʽʶ����
    Frame_SyncMode = 0xA9   // ͬ��ģʽʶ����
};

enum {
    STA_None = 0, // ����״̬
    STA_Head,     // ���յ�֡ͷ
    STA_Point,    // ��ģʽ
    STA_Sync,     // ͬ��ģʽ
    STA_SyncData, // ͬ��ģʽ����
};

enum Result {
    Ok = 0,
    Error,
    Done,
};

static double data2Double(uint32_t value, int type)
{
    double d;
    union {
        uint32_t i;
        float f;
    } temp;

    switch (type) {
    case 0: // float
        temp.i = value;
        d = temp.f;
        break;
    case 1: // int8
        d = (int8_t)value;
        break;
    case 2: // int16
        d = (int16_t)value;
        break;
    case 3: // int32
        d = (int32_t)value;
        break;
    default:
        d = 0.0;
    }
    return d;
}

// ����һ��������, ����������
static Result getPointData(char &ch, double &value, uint8_t byte)
{
    static char channel;
    static uint32_t data;
    static int count, len, type;
    const int bytes[] = { 4, 1, 2, 4 }; // �������͵��ֽ���

    if (count == 0) { // ��һ���ֽ����������ͺ�ͨ����Ϣ
        channel = byte & 0x0F; // ͨ��ֵ
        // tpye: 0: float, 1: int8, 2: int16, 3: int32
        type = byte >> 4;
        if (type > 3) { // �������ʹ���
            count = 0;
            return Error;
        }
        len = bytes[type];
    } else { // ���漸���ֽ�������
        data = (data << 8) | byte;
        if (count >= len) { // �������
            ch = channel;
            value = data2Double(data, type);
            count = 0;
            data = 0;
            return Done;
        }
    }
    ++count;
    return Ok;
}

// ��֡�н���һ����, ��ʶ��֡ͷ
bool waveGetPointValue(char &ch, double &value, uint8_t byte)
{
    Result res;
    static int count, len, status = STA_None;

    // ����֡ͷ״̬��
    switch (status) {
    case STA_None:
        status = byte == Frame_Head ? STA_Head : STA_None;
        break;
    case STA_Head:
        /* byte == Frame_PointMode -> status = STA_Point
         * byte == Frame_SyncMode -> status = STA_Sync
         * else -> status = STA_None
         */
        status = byte == Frame_PointMode ? STA_Point
            : (byte == Frame_SyncMode ? STA_Sync : STA_None);
        break;
    case STA_Point:
        res = getPointData(ch, value, byte);
        switch (res) {
        case Ok: // ���ڽ�������
            break;
        case Error: // ���������¿�ʼ����
            status = STA_None;
            break;
        case Done: // ������ʼ��״̬������true
            status = STA_None;
            return true;
        }
        break;
    case STA_Sync:
        count = 0;
        len = byte;
        // ���len > 64��֡���ȴ���, ������ƥ��֡, ����ת��STA_SyncData״̬
        status = len <= 64 ? STA_SyncData : STA_None;
        break;
    case STA_SyncData:
        if (++count >= len) { // �����ﵽ֡����˵��֡����, ����״̬
            status = STA_None;
        }
        res = getPointData(ch, value, byte);
        switch (res) {
        case Ok: // ���ڽ�������
            break;
        case Error: // ���������¿�ʼ����
            status = STA_None;
            break;
        case Done: // ��������true
            return true;
        }
        break;
    default: // �쳣�����λ״̬
        status = STA_None;
    }
    return false;
}
