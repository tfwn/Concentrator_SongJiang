using System;
using System.Collections;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading;
using System.Windows.Forms;
using System.IO;
using System.IO.Ports;
using UsbLibrary;
using Common;
using SR6009_Concentrator_Tools.FunList;
using System.Net;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Diagnostics;
//using Microsoft.Office.Core;

namespace SR6009_Concentrator_Tools
{
    public partial class FrmMain : Form
    {
        private enum enCmd
        {
            读表具数据 = 0x01,
            读冻结数据_6009 = 0x02,
            读表具冻结数据 = 0x05,

            松江抄表 = 0x31,
            打开表端实时抄表功能 = 0x32,
            关闭表端实时抄表功能 = 0x33,
            打开GprsDebug功能 = 0x34,
            关闭GprsDebug功能 = 0x35,
            设置SIM卡号 = 0x36,
            读集中器信道设置方式 = 0x72,
            写集中器信道设置方式 = 0x73,
            获取SIM卡号 = 0x74,

            复合命令字1 = 0x09,

            集中器版本信息 = 0x40,
            读集中器地址 = 0x41,
            写集中器地址 = 0x42,
            读集中器时钟 = 0x43,
            写集中器时钟 = 0x44,
            读Gprs参数 = 0x45,
            写Gprs参数 = 0x46,
            读Gprs信号强度 = 0x47,
            集中器初始化 = 0x48,
            读集中器工作模式参数 = 0x49,
            写集中器工作模式参数 = 0x4A,
            集中器请求时间 = 0x4B,
            集中器重新启动 = 0x4C,
            集中器数据转发 = 0x4D,

            读表具数量 = 0x50,
            读表具档案信息 = 0x51,
            写表具档案信息 = 0x52,
            删除表具档案信息 = 0x53,
            修改表具档案信息 = 0x54,
            读表具自定义路由信息 = 0x55,
            写表具自定义路由信息 = 0x56,
            批量读设备自定义路由信息 = 0x57,
            批量写设备自定义路由信息 = 0x58,

            定时定量数据主动上传 = 0x61,
            冻结数据主动上传 = 0x62,
            读集中器中的定时定量数据 = 0x63,
            读集中器中的冻结数据 = 0x64,
            批量读取集中器中的定时定量数据 = 0x65,
            批量读取集中器中的冻结数据 = 0x66,

            集中器程序升级 = 0xF1,
            集中器监控控制 = 0xF2,
            存储器检查 = 0xF3,
            指令空闲 = 0xFF,
        };
        private struct ProtocolStruct
        {
            public int PacketLength;                                        // 接收到消息长度
            public byte PacketProperty;                                     // 报文标识
            public byte TaskIDByte;                                         // 任务号字节
            public byte CommandIDByte;                                      // 命令字节
            public byte DeviceTypeByte;                                     // 设备类型字节
            public byte LifeCycleByte;                                      // 生命周期字节
            public byte RouteLevelByte;                                     // 路径级数信息
            public byte RouteCurPos;                                        // 路由当前位置
            public byte[] RoutePathList;                                    // 传输路径列表
            public byte[] DataBuf;                                          // 数据域字节
            public byte DownSignalStrengthByte;                             // 下行信号强度
            public byte UpSignalStrengthByte;                               // 上行信号强度
            public byte Crc8Byte;                                           // 校验字
            public byte EndByte;                                            // 结束符
            public bool isSuccess;                                          // 是否成功
        };
        private string[] strMeterType = new string[] {
            "冷水表",
            "热水表",
            "燃气表",
            "电能表",
            "中继器",
        };
        DataTable Document = new DataTable();
        private const string strNoPath = "无路由";
        public static byte AddrLength = 6;                                  // 地址长度
        private int CommDelayTime = 1200;                                   // 通信延时时间
        private const int MaxNodeNumOnePath = 7;                            // 路径中包含最大节点数(包含集中器和目标节点)
        private int _50MsTimer = 0;                                         // 50毫秒定时器
        private int _500MsTimer = 0;                                        // 500毫秒定时器
        private int[] PathLevel = new int[2] { 2, 2 };                      // 每条路径的级数(包括集中器和目标节点)
        private const byte UpDir = 0x80;                                    // 上行数据
        private const byte AckFrm = 0x40;                                   // 应答帧
        private const byte EnCrypt = 0x20;                                  // 加密帧
        private const byte NeedAck = 0x10;                                  // 需要应答
        private const byte SyncWord0 = 0xD3;                                // 同步字0
        private const byte SyncWord1 = 0x91;                                // 同步字1
        private const int MaxNodeNum = 1024;                                // 最大表具数量
        private enCmd Command = enCmd.指令空闲;                             // 当前执行指令
        private enCmd LastCommand = enCmd.指令空闲;                         // 上一个执行的指令
        private int ReadPos = 0;                                            // 读取档案时的位置指示
        private int WritePos = 0;                                           // 写入档案时的位置指示
        private string strLocalAddr = "";                                   // 本机地址
        private byte bFrameSn = 0;                                          // 任务号
        private int PortBufRdPos = 0;                                       // 接收缓冲器读位置
        private int PortBufWrPos = 0;                                       // 接收缓冲器写位置
        private byte[] PortRxBuf = new Byte[2000];                          // 接收缓冲器定义
        private delegate void SerialDataRecievedEventHandler(object sender, SerialDataReceivedEventArgs e);
        public static string strConfigFile = System.Windows.Forms.Application.StartupPath + @"\Config.xml";
        public string strCurDataFwdAddr = null;                             // 当前数据转发的地址
        public static string[] strDataFwdNodeAddrList;                      // 数据转发时表具地址列表
        private int iLastDataFwdNodeCount = 0;                              // 上一个数据转发队列数量
        private string strCurNodeAddr = "";                                 // 当前操作的节点地址
        private string strDataType = "定量数据";                            // 当前数据类型
        string strColumnIndex = "null";                                     // 列序号定义

        #region 窗口控制
        public FrmMain()
        {
            InitializeComponent();
        }
        private void FrmMain_Load(object sender, EventArgs e)
        {
            this.Text = "荣旗集中器参数设置工具V1.30.171204";
            AddStringToCommBox(false, " <<<====== 荣旗集中器参数设置工具V1.30.171204 ======>>>", null, Color.DarkGreen);
            this.Width = 1360;
            GetLocalIP();
            //cmbTimeType.SelectedIndex = 0;
            cmbNodeType.Items.AddRange(strMeterType);
            cmbOperateCmd.SelectedIndex = -1;
            CreateUsbThread();

            Document.TableName = "表具档案和抄表数据";
            Document.Columns.Add("序号", typeof(string));
            Document.Columns.Add("表具地址", typeof(string));
            Document.Columns.Add("类型", typeof(string));
            Document.Columns.Add("结果", typeof(string));
            Document.Columns.Add("路径0", typeof(string));
            Document.Columns.Add("路径1", typeof(string));
            Document.Columns.Add("场强↓", typeof(string));
            Document.Columns.Add("场强↑", typeof(string));
            Document.Columns.Add("报警信息", typeof(string));
            Document.Columns.Add("阀门状态", typeof(string));
            Document.Columns.Add("电压", typeof(string));
            Document.Columns.Add("温度", typeof(string));
            Document.Columns.Add("信噪比", typeof(string));
            Document.Columns.Add("信道", typeof(string));
            Document.Columns.Add("版本", typeof(string));
            Document.Columns.Add("读取时间", typeof(string));
            Document.Columns.Add("正转用量", typeof(string));
            Document.Columns.Add("反转用量", typeof(string));
            Document.Columns.Add("起始序号", typeof(string));
            Document.Columns.Add("冻结时间", typeof(string));
            Document.Columns.Add("冻结方式", typeof(string));
            Document.Columns.Add("冻结数量", typeof(string));
            Document.Columns.Add("时间间隔", typeof(string));
            Document.Columns.Add("冻结数据", typeof(string));

        }
        private void GetLocalIP()
        {
            try
            {
                string name = Dns.GetHostName();
                IPAddress[] ipadrlist = Dns.GetHostAddresses(name);
                foreach (IPAddress ipa in ipadrlist)
                {
                    if (ipa.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork)
                    {
                        string[] strIpSplit = ipa.ToString().Split('.');
                        strLocalAddr = "";
                        for (int iLoop = 0; iLoop < strIpSplit.Length; iLoop++)
                        {
                            strLocalAddr += strIpSplit[iLoop].PadLeft(3, '0');
                        }
                        strLocalAddr = strLocalAddr.PadLeft(AddrLength * 2, '0');
                        strLocalAddr = strLocalAddr.Substring(0, AddrLength * 2);
                        tsLocalAddress.Text = "本机地址：" + strLocalAddr;
                        return;
                    }
                }
            }
            catch
            {
                strLocalAddr = "111111111111";
                tsLocalAddress.Text = "本机地址：" + strLocalAddr;
            }
        }

        private void FrmMain_SizeChanged(object sender, EventArgs e)
        {
            //if (scon2.Panel2.Width > 30)
            //{
            //    if (tbPageRec.Parent != tbPageRecord)
            //    {
            //        tbPageDocument.Controls.Remove(tbPageRec);
            //        tbPageRecord.Controls.Add(tbPageRec);
            //    }
            //}
            //else
            //{
            //    if (tbPageRec.Parent != tbPageDocument)
            //    {
            //        tbPageRecord.Controls.Remove(tbPageRec);
            //        tbPageDocument.Controls.Add(tbPageRec);
            //    }
            //}
            if (this.Size.Height > this.MinimumSize.Height)
            {
                //gpbGprsInfo.Size = new System.Drawing.Size(352, this.Size.Height - 768 + 279);
                gpbDataFwd.Size = new System.Drawing.Size(352, this.Size.Height - 768 + 490);
                gpbFunList.Size = new System.Drawing.Size(329, this.Size.Height - 768 + 405);
            }
            else
            {
                //gpbGprsInfo.Size = new System.Drawing.Size(352, 279);
                gpbDataFwd.Size = new System.Drawing.Size(352, 490);
                gpbFunList.Size = new System.Drawing.Size(329, 405);
            }
        }
        private void scon2_splitterMoved(object sender, SplitterEventArgs e)
        {
            //if (scon2.Panel2.Width > 30)
            //{
            //    if (tbPageRec.Parent != tbPageRecord)
            //    {
            //        tbPageDocument.Controls.Remove(tbPageRec);
            //        tbPageRecord.Controls.Add(tbPageRec);
            //    }
            //}
            //else
            //{
            //    if (tbPageRec.Parent != tbPageDocument)
            //    {
            //        tbPageRecord.Controls.Remove(tbPageRec);
            //        tbPageDocument.Controls.Add(tbPageRec);
            //    }
            //}
        }
        #endregion

        #region 端口操作
        private void cmbPort_Click(object sender, EventArgs e)
        {
            cmbPort.Items.Clear();
            //foreach (string portName in SerialPort.GetPortNames())
            //{
            //    cmbPort.Items.Add(portName);
            //}
            if (true == _isConnect)
            {
                cmbPort.Items.Add("USB");
                cmbPort.SelectedIndex = 0;
                //              cmbPort.Text = "USB";
            }
        }
        private void cmbPort_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (cmbPort.Text == "USB")
            {
                cmbComType.Enabled = false;
                cmbComType.Text = "USB通信";
            }
            else
            {
                cmbComType.Enabled = true;
            }
        }
        private void cmbComType_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (cmbComType.Text == "无线通信")
            {
                CommDelayTime = 3000;
            }
            else
            {
                CommDelayTime = 3000;
            }
        }
        private void btPortCtrl_Click(object sender, EventArgs e)
        {
            if (cmbPort.SelectedIndex < 0 || cmbPort.Text == "")
            {
                MessageBox.Show("请先选择通信端口！", "错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }
            if (cmbComType.Text == "")
            {
                MessageBox.Show("请选择通信类型！", "错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }
            if ((cmbComType.Text == "USB通信" && cmbPort.Text != "USB") || (cmbComType.Text != "USB通信" && cmbPort.Text == "USB"))
            {
                MessageBox.Show("当前使用端口和通信类型不匹配！", "错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }
            if (cmbPort.Text == "USB")
            {
                if (btPortCtrl.Text == "打开端口")
                {
                    if (true == _isConnect)
                    {
                        btPortCtrl.Text = "关闭端口";
                        cmbPort.Enabled = false;
                        cmbComType.Enabled = false;
                        AddStringToCommBox(true, "打开" + cmbPort.Text + "端口成功，通信类型为" + cmbComType.Text + "！", null, Color.Black);
                        AddStringToCommBox(false, "\n****************************************", null, Color.Black);
                        lbCurState.Text = "设备状态：空闲";
                        tsComPortInfo.Text = "通信端口：" + cmbPort.Text + " 通信类型：" + cmbComType.Text + " 状态：打开";


                        if (false == DeviceStatus(true, true, false))
                        {
                            return;
                        }
                        int iLen = 0;
                        byte[] path = new byte[AddrLength * 2 + 1];
                        path[iLen++] = 0x02;
                        iLen += GetByteAddrFromString(strLocalAddr, path, iLen);
                        for (int iLoop = 0; iLoop < AddrLength; iLoop++)
                        {
                            path[iLen++] = 0xD5;
                        }
                        byte[] txBuf = ConstructTxBuffer(enCmd.读集中器地址, NeedAck, path, null);
                        DataTransmit(txBuf);
                        AddStringToCommBox(false, "\n<<<-----------------读集中器编号----------------->>>", null, Color.DarkGreen, true);
                        AddStringToCommBox(true, "发送：", txBuf, Color.Black);
                        lbCurState.Text = "设备状态：读集中器编号";
                        ProgressBarCtrl(0, 0, CommDelayTime / timer.Interval);


                    }
                    else
                    {
                        btPortCtrl.Text = "打开端口";
                        cmbPort.Enabled = true;
                        cmbComType.Enabled = true;
                        AddStringToCommBox(true, "打开" + cmbPort.Text + "端口失败！", null, Color.Red);
                        AddStringToCommBox(false, "\n****************************************", null, Color.Black);
                        lbCurState.Text = "设备状态：通信端口关闭";
                        tsComPortInfo.Text = "通信端口：" + cmbPort.Text + " 通信类型：" + cmbComType.Text + " 状态：关闭";
                    }
                }
                else
                {
                    btPortCtrl.Text = "打开端口";
                    cmbPort.Enabled = true;
                    cmbComType.Enabled = true;
                    AddStringToCommBox(true, "关闭" + cmbPort.Text + "端口成功！", null, Color.Black);
                    AddStringToCommBox(false, "\n****************************************", null, Color.Black);
                    lbCurState.Text = "设备状态：通信端口关闭";
                    tsComPortInfo.Text = "通信端口：" + cmbPort.Text + " 通信类型：" + cmbComType.Text + " 状态：关闭";
                }
                return;
            }

        }
        private void serialPort_DataTransmit(byte[] buf, int len)
        {
            try
            {
                serialPort.Write(buf, 0, len);
            }
            catch (System.Exception ex)
            {
                Command = enCmd.指令空闲;
                MessageBox.Show("串口通信出现异常:" + ex.Message, "出错啦", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }
        private void serialPort_DataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            int iRead;

            if (InvokeRequired)
            {
                try
                {
                    Invoke(new SerialDataRecievedEventHandler(serialPort_DataReceived), new object[] { sender, e });
                }
                catch (Exception ex)
                {
                    Console.WriteLine(ex.ToString());
                }
            }
            else
            {
                try
                {
                    while (serialPort.BytesToRead > 0)
                    {
                        iRead = serialPort.ReadByte();
                        PortRxBuf[PortBufWrPos] = (byte)iRead;
                        PortBufWrPos = (PortBufWrPos + 1) % PortRxBuf.Length;
                    }
                }
                catch (Exception ex)
                {
                    Console.WriteLine(ex.ToString());
                }
            }
        }
        #endregion

        #region USB通信部分
        protected Thread ConnectUsbThread = null;
        private bool _isConnect = false;
        public bool isConnect
        {
            set
            {
                _isConnect = value;
                SetConnectStatusText(value);
            }
            get { return _isConnect; }
        }
        protected void SetConnectStatusText(bool isSuccess)
        {
            if (this.InvokeRequired)
            {
                SetConnectStatusTextCallBack st = new SetConnectStatusTextCallBack(SetConnectStatusText);
                this.Invoke(st, new object[] { isSuccess });
            }
            else
            {
                if (isSuccess == false)
                {
                    if (cmbPort.Text == "USB" && btPortCtrl.Text == "关闭端口")
                    {
                        MessageBox.Show("USB通信出现异常！", "USB端错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        btPortCtrl.Text = "打开端口";
                        cmbPort.Enabled = true;
                        cmbComType.Enabled = true;
                    }
                }
            }
        }
        protected delegate void SetConnectStatusTextCallBack(bool isSuccess);
        void CreateUsbThread()
        {
            ConnectUsbThread = new Thread(ConnectUsbWork);
            ConnectUsbThread.IsBackground = true;
            ConnectUsbThread.Priority = ThreadPriority.Normal;
            ConnectUsbThread.Start();
        }
        void ConnectUsbWork()
        {
            while (true)
            {
                ConnectUsb();
                Thread.Sleep(2000);
            }
        }
        private delegate void ConnectUsbCallBack(); //连接USB
        protected void ConnectUsb()
        {
            if (this.InvokeRequired)//等待异步
            {
                ConnectUsbCallBack fc = new ConnectUsbCallBack(ConnectUsb);
                this.Invoke(fc); //通过代理调用刷新方法
            }
            else
            {
                try
                {
                    SetErrorText("");
                    this.usbHidPort.ProductId = Convert.ToInt32("5750", 16);
                    this.usbHidPort.VendorId = Convert.ToInt32("0483", 16);
                    this.usbHidPort.CheckDevicePresent();
                }
                catch (Exception ex)
                {
                    SetErrorText(ex.ToString());
                }
            }
        }
        private void usbHidPort_OnSpecifiedDeviceArrived(object sender, EventArgs e)
        {
            isConnect = true;
        }
        private void usbHidPort_OnSpecifiedDeviceRemoved(object sender, EventArgs e)
        {
            isConnect = false;
        }
        private void usbHidPort_OnDataRecieved(object sender, DataRecievedEventArgs args)
        {
            int loop;

            if (InvokeRequired)
            {
                try
                {
                    Invoke(new DataRecievedEventHandler(usbHidPort_OnDataRecieved), new object[] { sender, args });
                }
                catch (Exception ex)
                {
                    Console.WriteLine(ex.ToString());
                }
            }
            else
            {
                for (loop = 0; loop < 64; loop++)
                {
                    PortRxBuf[PortBufWrPos] = args.data[loop + 1];
                    PortBufWrPos = (PortBufWrPos + 1) % PortRxBuf.Length;
                }
            }
        }
        public void usbHidPort_OnDataTransmit(byte[] buf, int len)
        {
            byte[] usbBuf = new byte[65];
            byte loop = 0;

            if (isConnect)
            {
                loop = (byte)((len + 1) / 64);
                if (((len + 1) % 64) != 0)
                {
                    loop++;
                }
                if (this.usbHidPort.SpecifiedDevice != null)
                {
                    for (byte i = 0; i < loop; i++)
                    {
                        Array.Clear(usbBuf, 0, usbBuf.Length);
                        for (int j = 0; j < 64; j++)
                        {
                            if (i * 64 + j >= buf.Length)
                            {
                                break;
                            }
                            usbBuf[j + 1] = buf[i * 64 + j];
                        }
                        this.usbHidPort.SpecifiedDevice.SendData(usbBuf);
                    }
                }
            }
            else
            {
                //MessageBox.Show(WindowHandler.GetLangValue("USBNOTCONNECTED"),
                //             Util.SystemTitle,
                //              MessageBoxButtons.OK,
                //             MessageBoxIcon.Error
                //              );
                //MessageBoxEx.Show(WindowHandler.GetLangValue("USBNOTCONNECTED"),
                //       Util.SystemTitle, MessageBoxIcon.Error);
            }
        }
        private delegate void SetErrorTextCallBack(string MsgText);
        protected void SetErrorText(string MsgText)
        {
            if (this.InvokeRequired)
            {
                SetErrorTextCallBack st = new SetErrorTextCallBack(SetErrorText);
                this.Invoke(st, new object[] { MsgText });
            }
            else
            {
                //this.tsConnectStatus.Text = MsgText;
            }
        }
        #endregion

        #region 定时器
        private bool flag = false;
        private void timer_Tick(object sender, EventArgs e)
        {
            int len, sum, loop;

            if (Command != enCmd.指令空闲)
            {
                if (prgBar.Value < prgBar.Maximum)
                {
                    prgBar.Value++;
                }
                else
                {
                    if (Command == enCmd.集中器数据转发)
                    {
                        btRunCmd.Text = "执行命令";
                        gpbFunList.Enabled = true;
                        cbRunNodeAddr.Enabled = true;
                        cmbOperateCmd.Enabled = true;
                    }
                    else if (flag == false && (
                        Command == enCmd.批量读取集中器中的冻结数据 ||
                        Command == enCmd.批量读取集中器中的定时定量数据 ||
                        Command == enCmd.批量读设备自定义路由信息 ||
                        Command == enCmd.批量写设备自定义路由信息 ||
                        Command == enCmd.读表具档案信息 ||
                        Command == enCmd.写表具档案信息))
                    {
                        flag = true;
                        LastCommand = Command;
                        if (DialogResult.Retry == MessageBox.Show("通信失败，继续重试还是取消任务？", "通信中断", MessageBoxButtons.RetryCancel, MessageBoxIcon.Question))
                        {
                            flag = false;
                            return;
                        }
                    }
                    Command = enCmd.指令空闲;
                    ProgressBarCtrl(0, 0, 1000);
                    AddStringToCommBox(false, "\n执行结果：失败", null, Color.Red);
                }
            }

            if (_50MsTimer++ > 50 / timer.Interval)
            {
                _50MsTimer = 0;
                if (strDataFwdNodeAddrList != null && strDataFwdNodeAddrList.Length > 0)
                {
                    btRunCmd.Enabled = true;
                    if (strDataFwdNodeAddrList.Length != iLastDataFwdNodeCount)
                    {
                        iLastDataFwdNodeCount = strDataFwdNodeAddrList.Length;
                        lbNodeAddrList.Text = "表具地址列表[" + strDataFwdNodeAddrList.Length.ToString("D") + "]";
                        cbRunNodeAddr.Items.Clear();
                        cbRunNodeAddr.Items.AddRange(strDataFwdNodeAddrList);
                        cbRunNodeAddr.SelectedIndex = 0;
                    }
                }
                else if (cbRunNodeAddr.Text.Length > 0)
                {

                }
                else
                {
                    btRunCmd.Enabled = false;
                    cbRunNodeAddr.Items.Clear();
                    lbNodeAddrList.Text = "表具地址列表[0]";
                }
                while (true)
                {
                    len = (PortBufWrPos >= PortBufRdPos) ? (PortBufWrPos - PortBufRdPos) : (PortRxBuf.Length - PortBufRdPos + PortBufWrPos);
                    if (len < 4)
                    {
                        break;
                    }
                    if (cmbPort.Text == "" || btPortCtrl.Text == "打开端口")
                    {
                        PortBufRdPos = PortBufWrPos = 0;
                        break;
                    }

                    if (PortRxBuf[PortBufRdPos % PortRxBuf.Length] != SyncWord0 || PortRxBuf[(PortBufRdPos + 1) % PortRxBuf.Length] != SyncWord1)
                    {
                        PortBufRdPos = (UInt16)((PortBufRdPos + 1) % PortRxBuf.Length);
                        continue;
                    }
                    sum = PortRxBuf[(PortBufRdPos + 2) % PortRxBuf.Length] + (PortRxBuf[(PortBufRdPos + 3) % PortRxBuf.Length] & 0x03) * 256 + 2;
                    if (sum > 1000)
                    {
                        PortBufRdPos = (UInt16)((PortBufRdPos + 1) % PortRxBuf.Length);
                        continue;
                    }
                    if (cmbComType.Text != "无线通信")
                    {
                        sum += 2;
                    }
                    if (len < sum)
                    {
                        break;
                    }
                    byte[] rxBuf = new Byte[sum];
                    for (loop = 0; loop < sum; loop++)
                    {
                        rxBuf[loop] = PortRxBuf[(PortBufRdPos + loop) % PortRxBuf.Length];
                    }
                    PortBufRdPos = (PortBufRdPos + loop) % PortRxBuf.Length;
                    ExplainPacket(rxBuf);
                }
            }
            if (_500MsTimer++ > 500 / timer.Interval)
            {
                _500MsTimer = 0;
                lbCurTime.Text = DateTime.Now.ToString(" yyyy年MM月dd日 HH:mm:ss  ");
            }
        }
        #endregion

        #region 共用函数
        private bool DeviceStatus(bool bCheckPort, bool bCheckCmd, bool bCheckConcAddr)
        {
            if (true == bCheckPort && btPortCtrl.Text == "打开端口")
            {
                MessageBox.Show("请先打开通信端口!", "警告", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return false;
            }
            if (true == bCheckCmd && enCmd.指令空闲 != Command)
            {
                MessageBox.Show("当前指令运行中,请稍后再试!", "警告", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return false;
            }
            if (true == bCheckConcAddr && tbConcAddr.Text.Length != AddrLength * 2)
            {
                MessageBox.Show("请先读出集中器编号，然后再试！", "错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return false;
            }
            return true;
        }
        public static string GetStringAddrFromByte(byte[] DataBuf, int iStart)
        {
            string strNodeAddr = "";
            for (int iLoop = 0; iLoop < AddrLength; iLoop++)
            {
                strNodeAddr += DataBuf[iStart + iLoop].ToString("X2");
            }
            return strNodeAddr;
        }
        private int GetByteAddrFromString(string strSource, byte[] DataBuf, int iStart)
        {
            int iPos = iStart;
            for (int iLoop = 0; iLoop < strSource.Length;)
            {
                DataBuf[iPos++] = Convert.ToByte(strSource.Substring(iLoop, 2), 16);
                iLoop += 2;
            }
            return (iPos - iStart);
        }
        private string GetDevTypeFromByte(byte TypeId)
        {
            switch (TypeId)
            {
                case 0x10: return "冷水表";
                case 0x20: return "热水表";
                case 0x30: return "燃气表";
                case 0x40: return "电能表";
                case 0xFD: return "中继器";
                default: return "未知类型";
            }
        }
        private byte GetDevTypeFromString(string strDevType)
        {
            switch (strDevType)
            {
                case "冷水表": return 0x10;
                case "热水表": return 0x20;
                case "燃气表": return 0x30;
                case "电能表": return 0x40;
                case "中继器": return 0xFD;
                default: return 0xFF;
            }
        }
        public static string GetErrorInfo(byte ErrCode)
        {
            switch (ErrCode)
            {
                case 0xAA: return "操作成功";
                case 0xAB: return "操作失败";
                case 0xAC: return "通讯失败";
                case 0xAD: return "命令下达成功";
                case 0xAE: return "数据格式错误";
                case 0xAF: return "时间异常";
                case 0xBA: return "对象不存在";
                case 0xBB: return "对象重复";
                case 0xBC: return "对象已满";
                case 0xBD: return "参数错误";
                case 0xCC: return "超时错误";
                case 0xCD: return "单轮运行超时错误";
                case 0xCE: return "正在执行";
                case 0xCF: return "操作已处理";
                case 0xD0: return "已应答";
                case 0xD1: return "抄读表数据错误";
                case 0xD2: return "没有此项功能";
                default: return "未知错误";
            }
        }
        private byte CalCRC8(byte[] dataBuf, int pos, int dataLen)
        {
            byte i, cCrc = 0;
            for (int m = 0; m < dataLen; m++)
            {
                cCrc ^= dataBuf[m + pos];
                for (i = 0; i < 8; i++)
                {
                    if ((cCrc & 0x01) != 0)
                    {
                        cCrc >>= 1;
                        cCrc ^= 0x8c;
                    }
                    else
                    {
                        cCrc >>= 1;
                    }
                }
            }
            return cCrc;
        }
        public static byte Hex2Bcd(byte hexVal)
        {
            byte a, b;
            a = (byte)(hexVal / 10);
            b = (byte)(hexVal % 10);
            return (byte)(a << 4 | b);
        }
        public static byte Bcd2Hex(byte bcdVal)
        {
            byte a, b;
            a = (byte)(bcdVal >> 4 & 0x0F);
            b = (byte)(bcdVal & 0x0F);
            return (byte)(a * 10 + b);
        }
        public static string Byte6ToUint64(byte[] SrcBuf, int StartIndex)
        {
            UInt64 integer = 0;
            UInt32 fraction = 0;

            for (int iLoop = 0; iLoop < 4; iLoop++)
            {
                integer <<= 8;
                integer += SrcBuf[StartIndex + 4 - iLoop - 1];
            }
            fraction = (UInt32)(SrcBuf[StartIndex + 4] + SrcBuf[StartIndex + 5] * 256);
            return (integer.ToString("D") + "." + fraction.ToString("D"));
        }
        private void tbAddress_KeyPress(object sender, KeyPressEventArgs e, int iLen, string strFilter)
        {
            TextBox tbAddress = (TextBox)sender;

            if (tbAddress.ReadOnly == true)
            {
                return;
            }
            if (strFilter.IndexOf(e.KeyChar) < 0)
            {
                e.Handled = true;
                return;
            }
            if (e.KeyChar == '\r')
            {
                tbAddress.Text = tbAddress.Text.PadLeft(iLen, '0');
                e.Handled = true;
            }
            if (tbAddress.Text.Length >= iLen && e.KeyChar != '\b')
            {
                if (tbAddress.SelectionLength == 0)
                {
                    e.Handled = true;
                }
            }
        }
        #endregion

        #region 协议解析
        private byte[] ConstructTxBuffer(enCmd Cmd, byte Option, byte[] Path, byte[] DataField)
        {
            byte bCrc;
            int iLen = 0;
            try
            {
                byte[] buf = new byte[1500];
                // 同步字
                buf[iLen++] = SyncWord0;
                buf[iLen++] = SyncWord1;
                // 包长度,后面填充
                iLen += 2;
                // 报文标识
                buf[iLen++] = Option;
                // 帧序号
                buf[iLen++] = bFrameSn++;
                // 命令字
                buf[iLen++] = (byte)Cmd;
                Command = Cmd;
                // 设备类型
                buf[iLen++] = 0xFA;
                // 生命周期
                buf[iLen++] = 0x3F;
                // 传输路径
                if (Path == null)
                {
                    buf[iLen++] = 0x02;
                    iLen += GetByteAddrFromString(strLocalAddr, buf, iLen);
                    iLen += GetByteAddrFromString(tbConcAddr.Text, buf, iLen);
                }
                else
                {
                    Array.Copy(Path, 0, buf, iLen, Path.Length);
                    iLen += Path.Length;
                }
                // 数据域
                if (DataField != null)
                {
                    Array.Copy(DataField, 0, buf, iLen, DataField.Length);
                    iLen += DataField.Length;
                }
                // 上行信号强度
                buf[iLen++] = 0;
                // 下行信号强度
                buf[iLen++] = 0;
                // 包长度
                buf[2] = (byte)(iLen);
                buf[3] = (byte)(iLen >> 8);
                // 校验字
                bCrc = CalCRC8(buf, 2, iLen - 2);
                buf[iLen++] = bCrc;
                // 结束符
                buf[iLen++] = 0x16;
                // 其他参数
                buf[iLen++] = 0x00;
                buf[iLen++] = 9 + 16;
                buf[iLen++] = 3;
                // 调整数据大小
                Array.Resize(ref buf, iLen);
                return buf;
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.ToString());
            }
            return null;
        }
        public void DataTransmit(byte[] dataBuf)
        {
            if (cmbComType.Text == "USB通信")
            {
                usbHidPort_OnDataTransmit(dataBuf, dataBuf.Length);
            }
            else
            {
                serialPort_DataTransmit(dataBuf, dataBuf.Length);
            }
        }
        private ProtocolStruct ExtractRxBuffer(byte[] Buffer)
        {
            ProtocolStruct proStruct = new ProtocolStruct();
            int iLoop, packPos, startPos;
            byte calCrc;

            try
            {
                if (Buffer[0] == SyncWord0 && Buffer[1] == SyncWord1)
                {
                    startPos = 2;
                }
                else
                {
                    startPos = 0;
                }
                packPos = startPos;
                // 包长度
                proStruct.PacketLength = Buffer[packPos++] + (Buffer[packPos++] & 0x03) * 256;
                //报文标识
                proStruct.PacketProperty = Buffer[packPos++];
                // 任务号
                proStruct.TaskIDByte = Buffer[packPos++];
                // 命令字
                proStruct.CommandIDByte = Buffer[packPos++];
                // 设备类型
                proStruct.DeviceTypeByte = Buffer[packPos++];
                // 生命周期
                proStruct.LifeCycleByte = Buffer[packPos++];
                // 路径条数
                proStruct.RouteLevelByte = (Byte)(Buffer[packPos] & 0x0F);
                // 当前位置
                proStruct.RouteCurPos = (byte)(Buffer[packPos] >> 4 & 0x0F);
                // 传输路径
                packPos++;
                proStruct.RoutePathList = new byte[proStruct.RouteLevelByte * AddrLength];
                for (iLoop = 0; iLoop < proStruct.RoutePathList.Length; iLoop++)
                {
                    proStruct.RoutePathList[iLoop] = Buffer[packPos++];
                }
                // 数据域
                proStruct.DataBuf = new byte[proStruct.PacketLength - 4 - packPos + startPos];
                for (iLoop = 0; iLoop < proStruct.DataBuf.Length; iLoop++)
                {
                    proStruct.DataBuf[iLoop] = Buffer[packPos++];
                }
                // 下行信号强度
                proStruct.DownSignalStrengthByte = Buffer[packPos++];
                // 上行信号强度
                proStruct.UpSignalStrengthByte = Buffer[packPos++];
                // 校验字
                proStruct.Crc8Byte = Buffer[packPos++];
                // 结束字
                proStruct.EndByte = Buffer[packPos++];
                // 是否成功
                calCrc = CalCRC8(Buffer, startPos, proStruct.PacketLength - 2);
                proStruct.isSuccess = (calCrc == proStruct.Crc8Byte) ? true : false;

                return proStruct;
            }
            catch
            {
                proStruct.isSuccess = false;
                return proStruct;
            }
        }
        private void ExplainPacket(byte[] RecBuf)
        {
            ProtocolStruct rxStruct = ExtractRxBuffer(RecBuf);
            if (false == rxStruct.isSuccess)
            {
                AddStringToCommBox(true, "接收：", RecBuf, Color.Black);
                AddStringToCommBox(false, "<校验错误>", null, Color.Red);
                return;
            }
            else if (rxStruct.CommandIDByte == (byte)enCmd.集中器监控控制)
            {
                ExplainGprsMonitor(rxStruct);
                return;
            }
            AddStringToCommBox(true, "接收：", RecBuf, Color.Black);
            switch (Command)
            {
                case enCmd.读集中器地址:
                    ExplainReadConcAddr(rxStruct);
                    break;
                case enCmd.读Gprs参数:
                    ExplainReadGprsParam(rxStruct);
                    break;
                case enCmd.写Gprs参数:
                    ExplainSetGprsParam(rxStruct);
                    break;
                case enCmd.设置SIM卡号:
                    ExplainSetSimNum(rxStruct);
                    break;
                case enCmd.获取SIM卡号:
                    ExplainGetSimNum(rxStruct);
                    break;
                case enCmd.读集中器信道设置方式:
                    ExplainReadChannelParam(rxStruct);
                    break;
                case enCmd.写集中器信道设置方式:
                    ExplainWriteChannelParam(rxStruct);
                    break;
                default:
                    //ExplainOtherData(rxStruct);
                    break;
            }
        }
        #endregion

        #region 通讯信息
        private void ProgressBarCtrl(int iValue, int iMinValue, int iMaxValue)
        {
            prgBar.Minimum = iMinValue;
            prgBar.Maximum = iMaxValue;
            if (iValue >= iMinValue && iValue <= iMaxValue)
            {
                prgBar.Value = iValue;
            }
        }
        private void AddStringToCommBox(bool bNeedTime, string strInfo, byte[] buf, Color strColor, bool bClear = false)
        {
            int iLoop, iStart;

            if (bClear == true && tsmiAutoClear.Checked == true)
            {
                rtbCommMsg.Clear();
            }
            if (rtbCommMsg.Text.Length > rtbCommMsg.MaxLength - 100)
            {
                rtbCommMsg.Clear();
            }
            iStart = rtbCommMsg.Text.Length;
            if (true == bNeedTime)
            {
                rtbCommMsg.AppendText("\n[" + DateTime.Now.ToString("yyyy/MM/dd HH:mm:ss:fff") + "]");
            }
            if (null != buf)
            {
                for (iLoop = 0; iLoop < buf.Length; iLoop++)
                {
                    strInfo += buf[iLoop].ToString("X2") + " ";
                }
                strInfo.Trim();
            }
            rtbCommMsg.AppendText(strInfo);
            rtbCommMsg.Select(iStart, rtbCommMsg.Text.Length);
            rtbCommMsg.SelectionColor = strColor;
            if (tsmiAutoScroll.Checked == true)
            {
                rtbCommMsg.ScrollToCaret();
            }
        }

        private void cntMenuStripCommInfo_Opening(object sender, CancelEventArgs e)
        {
            tsmiAutoScroll.Enabled = true;
            if ("1" == Common.XmlHelper.GetNodeDefValue(strConfigFile, "/Config/Global/CommonMsgAutoScroll", "1"))
            {
                tsmiAutoScroll.Checked = true;
            }
            else
            {
                tsmiAutoScroll.Checked = false;
            }
            if ("1" == Common.XmlHelper.GetNodeDefValue(strConfigFile, "/Config/Global/CommonMsgAutoClear", "0"))
            {
                tsmiAutoClear.Checked = true;
            }
            else
            {
                tsmiAutoClear.Checked = false;
            }
            if (rtbCommMsg.Text.Length == 0)
            {
                tsmiClearAll.Enabled = false;
                tsmiSaveRecord.Enabled = false;
            }
            else
            {
                tsmiClearAll.Enabled = true;
                tsmiSaveRecord.Enabled = true;
            }
        }
        private void tsmiClearAll_Click(object sender, EventArgs e)
        {
            rtbCommMsg.Clear();
        }
        private void tsmiAutoScroll_Click(object sender, EventArgs e)
        {
            Common.XmlHelper.SetNodeValue(strConfigFile, "/Config/Global", "CommonMsgAutoScroll", (tsmiAutoScroll.Checked == true ? "1" : "0"));
        }
        private void tsmiAutoClear_Click(object sender, EventArgs e)
        {
            Common.XmlHelper.SetNodeValue(strConfigFile, "/Config/Global", "CommonMsgAutoClear", (tsmiAutoClear.Checked == true ? "1" : "0"));
        }
        private void tsmiSaveRecord_Click(object sender, EventArgs e)
        {
            string strDirectory = "";
            string strFileName;

            if (rtbCommMsg.Text.Length == 0)
            {
                MessageBox.Show("没有通讯数据可以保存！", "错误");
                return;
            }
            strDirectory = Common.XmlHelper.GetNodeDefValue(strConfigFile, "/Config/Global/SaveCommMsgPath", System.Windows.Forms.Application.StartupPath);
            saveFileDlg.Filter = "*.txt(文本文件)|*.txt";
            saveFileDlg.DefaultExt = "txt";
            saveFileDlg.FileName = "通信记录_" + tbConcAddr.Text + "_" + DateTime.Now.ToString("yyMMddHHmm");
            if (saveFileDlg.ShowDialog() == DialogResult.Cancel)
            {
                return;
            }
            strFileName = saveFileDlg.FileName;
            if (strFileName.Length == 0)
            {
                return;
            }
            if (strDirectory != Path.GetDirectoryName(strFileName))
            {
                strDirectory = Path.GetDirectoryName(strFileName);
                Common.XmlHelper.SetNodeValue(strConfigFile, "/Config/Global", "SaveCommMsgPath", strDirectory);
            }
            try
            {
                StreamWriter sw = new StreamWriter(strFileName, true, System.Text.Encoding.UTF8);
                sw.WriteLine("\n******以下记录保存时间是" + DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss") + "******");
                string strTemp = rtbCommMsg.Text.Replace("\n", "\r\n");
                sw.Write(strTemp);
                sw.Close();
            }
            catch (SystemException ex)
            {
                Console.WriteLine(ex.ToString());
            }
        }
        #endregion

        #region 参数管理选项卡
        #region 读取集中器编号
        private void btReadConcAddr_Click(object sender, EventArgs e)
        {
            if (false == DeviceStatus(true, true, false))
            {
                return;
            }
            int iLen = 0;
            byte[] path = new byte[AddrLength * 2 + 1];
            path[iLen++] = 0x02;
            iLen += GetByteAddrFromString(strLocalAddr, path, iLen);
            for (int iLoop = 0; iLoop < AddrLength; iLoop++)
            {
                path[iLen++] = 0xD5;
            }
            byte[] txBuf = ConstructTxBuffer(enCmd.读集中器地址, NeedAck, path, null);
            DataTransmit(txBuf);
            AddStringToCommBox(false, "\n<<<-----------------读集中器编号----------------->>>", null, Color.DarkGreen, true);
            AddStringToCommBox(true, "发送：", txBuf, Color.Black);
            lbCurState.Text = "设备状态：读集中器编号";
            ProgressBarCtrl(0, 0, CommDelayTime / timer.Interval);
        }
        private void ExplainReadConcAddr(ProtocolStruct rxStruct)
        {
            try
            {
                if (rxStruct.CommandIDByte == (byte)enCmd.读集中器地址)
                {
                    tbConcAddr.Text = GetStringAddrFromByte(rxStruct.DataBuf, 0);
                    AddStringToCommBox(false, "\n执行结果：成功  集中器地址为：" + tbConcAddr.Text, null, Color.DarkBlue);
                    Command = enCmd.指令空闲;
                    lbCurState.Text = "设备状态：空闲";
                    ProgressBarCtrl(0, 0, 1000);
                    scon1.Panel2.Enabled = true;
                }
            }
            catch (Exception ex)
            {
                Command = enCmd.指令空闲;
                lbCurState.Text = "设备状态：空闲";
                ProgressBarCtrl(0, 0, 1000);
                MessageBox.Show("读取集中器编号出错：" + ex.Message);
            }
            return;
        }
        #endregion


        #region 其他参数操作
        private void tbConcAddr_KeyPress(object sender, KeyPressEventArgs e)
        {
            tbAddress_KeyPress(sender, e, AddrLength * 2, "0123456789\b\r\x03\x16\x18");
            if (tbConcAddr.Text.Length == AddrLength * 2)
            {
                scon1.Panel2.Enabled = true;
            }
            else
            {
                scon1.Panel2.Enabled = false;
            }
        }
        private void tbConcAddr_Leave(object sender, EventArgs e)
        {
            if (tbConcAddr.Text != "")
            {
                tbConcAddr.Text = tbConcAddr.Text.PadLeft(AddrLength * 2, '0');
                scon1.Panel2.Enabled = true;
            }
            else
            {
                scon1.Panel2.Enabled = false;
            }
        }

        #endregion
        #endregion

        #region GPRS参数选项卡
        #region 读取GPRS参数
        private void btReadGprsParam_Click(object sender, EventArgs e)
        {
            if (false == DeviceStatus(true, true, true))
            {
                return;
            }
            byte[] txBuf = ConstructTxBuffer(enCmd.读Gprs参数, NeedAck, null, null);
            DataTransmit(txBuf);
            AddStringToCommBox(false, "\n<<<-----------------读取GPRS参数----------------->>>", null, Color.DarkGreen, true);
            AddStringToCommBox(true, "发送：", txBuf, Color.Black);
            lbCurState.Text = "设备状态：读取GPRS参数";
            ProgressBarCtrl(0, 0, CommDelayTime / timer.Interval);
        }
        private void ExplainReadGprsParam(ProtocolStruct rxStruct)
        {
            try
            {
                if (rxStruct.CommandIDByte == (byte)enCmd.读Gprs参数)
                {
                    AddStringToCommBox(false, "\n执行结果：成功", null, Color.DarkBlue);
                    int iLen = 0;
                    string strInfo = "\n  " + lbServerIP0.Text + "：";
                    tbServerIp00.Text = rxStruct.DataBuf[iLen++].ToString("D3");
                    strInfo += tbServerIp00.Text + ".";
                    tbServerIp01.Text = rxStruct.DataBuf[iLen++].ToString("D3");
                    strInfo += tbServerIp01.Text + ".";
                    tbServerIp02.Text = rxStruct.DataBuf[iLen++].ToString("D3");
                    strInfo += tbServerIp02.Text + ".";
                    tbServerIp03.Text = rxStruct.DataBuf[iLen++].ToString("D3");
                    strInfo += tbServerIp03.Text + "  ";
                    tbServerPort0.Text = (rxStruct.DataBuf[iLen++] + rxStruct.DataBuf[iLen++] * 256).ToString("D");
                    strInfo += lbPort0.Text + "：" + tbServerPort0.Text;
                    AddStringToCommBox(false, strInfo, null, Color.DarkBlue);
                    strInfo = "\n  " + lbServerIP1.Text + "：";
                    tbServerIp10.Text = rxStruct.DataBuf[iLen++].ToString("D3");
                    strInfo += tbServerIp10.Text + ".";
                    tbServerIp11.Text = rxStruct.DataBuf[iLen++].ToString("D3");
                    strInfo += tbServerIp11.Text + ".";
                    tbServerIp12.Text = rxStruct.DataBuf[iLen++].ToString("D3");
                    strInfo += tbServerIp12.Text + ".";
                    tbServerIp13.Text = rxStruct.DataBuf[iLen++].ToString("D3");
                    strInfo += tbServerIp13.Text + "  ";
                    tbServerPort1.Text = (rxStruct.DataBuf[iLen++] + rxStruct.DataBuf[iLen++] * 256).ToString("D");
                    strInfo += lbPort1.Text + "：" + tbServerPort1.Text;
                    AddStringToCommBox(false, strInfo, null, Color.DarkBlue);
                    nudHeatBeat.Value = rxStruct.DataBuf[iLen++] / 6;
                    strInfo = "\n  " + lbHeatBeat.Text + "：" + nudHeatBeat.Value.ToString() + "分钟  ";
                    int count = rxStruct.DataBuf[iLen++];
                    tbApn.Text = System.Text.Encoding.Default.GetString(rxStruct.DataBuf, iLen, count);
                    if (tbApn.Text.Length > 0)
                    {
                        strInfo += lbApn.Text + "：" + tbApn.Text + "  ";
                    }
                    else
                    {
                        strInfo += lbApn.Text + "：未设置  ";
                    }
                    iLen += count;
                    count = rxStruct.DataBuf[iLen++];
                    tbUsername.Text = System.Text.Encoding.Default.GetString(rxStruct.DataBuf, iLen, count);
                    if (tbUsername.Text.Length > 0)
                    {
                        strInfo += lbUsername.Text + "：" + tbUsername.Text + "  ";
                    }
                    else
                    {
                        strInfo += lbUsername.Text + "：未设置  ";
                    }
                    iLen += count;
                    count = rxStruct.DataBuf[iLen++];
                    tbPassword.Text = System.Text.Encoding.Default.GetString(rxStruct.DataBuf, iLen, count);
                    if (tbPassword.Text.Length > 0)
                    {
                        strInfo += lbPassword.Text + "：" + tbPassword.Text;
                    }
                    else
                    {
                        strInfo += lbPassword.Text + "：未设置";
                    }
                    AddStringToCommBox(false, strInfo, null, Color.DarkBlue);

                    Command = enCmd.指令空闲;
                    lbCurState.Text = "设备状态：空闲";
                    ProgressBarCtrl(0, 0, 1000);
                }
            }
            catch (Exception ex)
            {
                Command = enCmd.指令空闲;
                lbCurState.Text = "设备状态：空闲";
                ProgressBarCtrl(0, 0, 1000);
                MessageBox.Show("读GPRS参数出错：" + ex.Message);
            }
            return;
        }
        #endregion
        #region 设置GPRS参数
        //private void btSetGprsParam_Click(object sender, EventArgs e)
        //{
        //    if (false == DeviceStatus(true, true, true))
        //    {
        //        return;
        //    }
        //    if (DialogResult.Cancel == MessageBox.Show("确认要写入这些GPRS参数吗？", "确认信息", MessageBoxButtons.OKCancel, MessageBoxIcon.Question))
        //    {
        //        return;
        //    }
        //    try
        //    {
        //        byte[] dataBuf = new byte[100];
        //        int iLen = 0;
        //        dataBuf[iLen++] = Convert.ToByte(tbServerIp00.Text);
        //        dataBuf[iLen++] = Convert.ToByte(tbServerIp01.Text);
        //        dataBuf[iLen++] = Convert.ToByte(tbServerIp02.Text);
        //        dataBuf[iLen++] = Convert.ToByte(tbServerIp03.Text);
        //        dataBuf[iLen++] = (byte)Convert.ToUInt16(tbServerPort0.Text);
        //        dataBuf[iLen++] = (byte)(Convert.ToUInt16(tbServerPort0.Text) >> 8);
        //        dataBuf[iLen++] = Convert.ToByte(tbServerIp10.Text);
        //        dataBuf[iLen++] = Convert.ToByte(tbServerIp11.Text);
        //        dataBuf[iLen++] = Convert.ToByte(tbServerIp12.Text);
        //        dataBuf[iLen++] = Convert.ToByte(tbServerIp13.Text);
        //        dataBuf[iLen++] = (byte)Convert.ToUInt16(tbServerPort1.Text);
        //        dataBuf[iLen++] = (byte)(Convert.ToUInt16(tbServerPort1.Text) >> 8);
        //        dataBuf[iLen++] = (byte)(nudHeatBeat.Value / 10);
        //        byte[] apn = System.Text.Encoding.Default.GetBytes(tbApn.Text);
        //        dataBuf[iLen++] = (byte)apn.Length;
        //        Array.Copy(apn, 0, dataBuf, iLen, apn.Length);
        //        iLen += apn.Length;
        //        byte[] username = System.Text.Encoding.Default.GetBytes(tbUsername.Text);
        //        dataBuf[iLen++] = (byte)username.Length;
        //        Array.Copy(username, 0, dataBuf, iLen, username.Length);
        //        iLen += username.Length;
        //        byte[] password = System.Text.Encoding.Default.GetBytes(tbPassword.Text);
        //        dataBuf[iLen++] = (byte)password.Length;
        //        Array.Copy(password, 0, dataBuf, iLen, password.Length);
        //        iLen += password.Length;
        //        Array.Resize(ref dataBuf, iLen);
        //        byte[] txBuf = ConstructTxBuffer(enCmd.写Gprs参数, NeedAck, null, dataBuf);
        //        DataTransmit(txBuf);
        //        AddStringToCommBox(false, "\n<<<-----------------设置GPRS参数----------------->>>", null, Color.DarkGreen, true);
        //        AddStringToCommBox(true, "发送：", txBuf, Color.Black);
        //        lbCurState.Text = "设备状态：设置GPRS参数";
        //        ProgressBarCtrl(0, 0, CommDelayTime / timer.Interval);
        //    }
        //    catch (Exception ex)
        //    {
        //        Command = enCmd.指令空闲;
        //        lbCurState.Text = "设备状态：空闲";
        //        ProgressBarCtrl(0, 0, 1000);
        //        MessageBox.Show("设置GPRS参数出错：" + ex.Message, "错误");
        //    }
        //}
        private void ExplainSetGprsParam(ProtocolStruct rxStruct)
        {
            try
            {
                if (rxStruct.CommandIDByte == (byte)enCmd.写Gprs参数)
                {
                    if (rxStruct.DataBuf[0] == 0xAA)
                    {
                        AddStringToCommBox(false, "\n执行结果：成功", null, Color.DarkBlue);
                        AddStringToCommBox(false, " 集中器可能会重新启动，并以新的IP地址连接服务器！", null, Color.Red);
                    }
                    else
                    {
                        AddStringToCommBox(false, "\n执行结果：失败  失败原因：" + GetErrorInfo(rxStruct.DataBuf[0]), null, Color.Red);
                    }
                    lbCurState.Text = "设备状态：空闲";
                    Command = enCmd.指令空闲;
                    ProgressBarCtrl(0, 0, 1000);
                }
            }
            catch (Exception ex)
            {
                Command = enCmd.指令空闲;
                lbCurState.Text = "设备状态：空闲";
                ProgressBarCtrl(0, 0, 1000);
                MessageBox.Show("设置GPRS参数出错：" + ex.Message);
            }
            return;
        }
        #endregion
        #region 其他GPRS参数
        private void ExplainGprsMonitor(ProtocolStruct rxStruct)
        {
            if (rxStruct.DataBuf[0] == 0)
            {
                if (rtbGprsMsg.Text.Length > rtbGprsMsg.MaxLength - 100)
                {
                    rtbGprsMsg.Clear();
                }
                rtbGprsMsg.AppendText(System.Text.Encoding.Default.GetString(rxStruct.DataBuf, 1, rxStruct.DataBuf.Length - 1));
                rtbGprsMsg.ScrollToCaret();
            }
        }
        private void tbServerIp_KeyPress(object sender, KeyPressEventArgs e)
        {
            TextBox tb = (TextBox)sender;

            if ("0123456789\b\r\x03\x16\x18".IndexOf(e.KeyChar) < 0)
            {
                e.Handled = true;
                return;
            }
            if (e.KeyChar == '\r')
            {
                SendKeys.Send("{TAB}");
                return;
            }
            if (tb.Text.Length >= 3 && e.KeyChar != '\b')
            {
                if (tb.SelectionLength == 0)
                {
                    e.Handled = true;
                }
            }
        }
        private void tbServerPort_KeyPress(object sender, KeyPressEventArgs e)
        {
            TextBox tb = (TextBox)sender;

            if ("0123456789\b\r\x03\x16\x18".IndexOf(e.KeyChar) < 0)
            {
                e.Handled = true;
                return;
            }
            if (e.KeyChar == '\r')
            {
                SendKeys.Send("{TAB}");
                return;
            }
            if (tb.Text.Length >= 5 && e.KeyChar != '\b')
            {
                if (tb.SelectionLength == 0)
                {
                    e.Handled = true;
                }
            }
        }
        private void tbApnInfo_KeyPress(object sender, KeyPressEventArgs e)
        {
            TextBox tb = (TextBox)sender;

            if (e.KeyChar == '\r')
            {
                SendKeys.Send("{TAB}");
                return;
            }
            if (tb.Text.Length >= 12 && e.KeyChar != '\b')
            {
                if (tb.SelectionLength == 0)
                {
                    e.Handled = true;
                }
            }
        }
        #endregion
        #endregion
        #region 数据抄读选项卡
        #region 单个表具定时定量数据
        private void btReadRealData_Click(object sender, EventArgs e)
        {
            if (false == DeviceStatus(true, true, true))
            {
                return;
            }
            if (tbCurAddr.Text.Length != AddrLength * 2)
            {
                MessageBox.Show("请先输入表具地址。", "表具地址错误");
                return;
            }
            try
            {
                byte[] dataBuf = new byte[AddrLength];
                strCurNodeAddr = tbCurAddr.Text;
                GetByteAddrFromString(strCurNodeAddr, dataBuf, 0);
                byte[] txBuf = ConstructTxBuffer(enCmd.读集中器中的定时定量数据, NeedAck, null, dataBuf);
                DataTransmit(txBuf);
                AddStringToCommBox(false, "\n<<<-----------------读取单个表具定时定量数据----------------->>>", null, Color.DarkGreen, true);
                AddStringToCommBox(true, "发送：", txBuf, Color.Black);
                lbCurState.Text = "设备状态：读取单个表具定时定量数据";
                ProgressBarCtrl(0, 0, CommDelayTime / timer.Interval);
            }
            catch (Exception ex)
            {
                Command = enCmd.指令空闲;
                lbCurState.Text = "设备状态：空闲";
                ProgressBarCtrl(0, 0, 1000);
                MessageBox.Show("读取单个表具定时定量数据时出错：" + ex.Message, "错误");
            }
        }

        #endregion
        #region 单个表具冻结数据
        private void btReadFreezeData_Click(object sender, EventArgs e)
        {
            if (false == DeviceStatus(true, true, true))
            {
                return;
            }
            if (tbCurAddr.Text.Length != AddrLength * 2)
            {
                MessageBox.Show("请先输入表具地址。", "表具地址错误");
                return;
            }
            try
            {
                byte[] dataBuf = new byte[AddrLength];
                strCurNodeAddr = tbCurAddr.Text;
                GetByteAddrFromString(strCurNodeAddr, dataBuf, 0);
                byte[] txBuf = ConstructTxBuffer(enCmd.读集中器中的冻结数据, NeedAck, null, dataBuf);
                DataTransmit(txBuf);
                AddStringToCommBox(false, "\n<<<-----------------读取单个表具冻结数据----------------->>>", null, Color.DarkGreen, true);
                AddStringToCommBox(true, "发送：", txBuf, Color.Black);
                lbCurState.Text = "设备状态：读取单个表具冻结数据";
                ProgressBarCtrl(0, 0, CommDelayTime / timer.Interval);
            }
            catch (Exception ex)
            {
                Command = enCmd.指令空闲;
                lbCurState.Text = "设备状态：空闲";
                ProgressBarCtrl(0, 0, 1000);
                MessageBox.Show("读取单个表具冻结数据时出错：" + ex.Message, "错误");
            }
        }

        #endregion
        #region 全部表具定时定量数据
        private void ReadAllRealDataProc()
        {
            byte[] dataBuf = new byte[3];
            dataBuf[0] = (byte)ReadPos;
            dataBuf[1] = (byte)(ReadPos >> 8);
            dataBuf[2] = (byte)("无线通信" == cmbComType.Text ? 5 : 25);
            byte[] txBuf = ConstructTxBuffer(enCmd.批量读取集中器中的定时定量数据, NeedAck, null, dataBuf);
            DataTransmit(txBuf);
            AddStringToCommBox(false, "\n<<<-----------------读取全部表具定时定量数据----------------->>>", null, Color.DarkGreen);
            AddStringToCommBox(true, "发送：", txBuf, Color.Black);
            lbCurState.Text = "设备状态：读取全部表具定时定量数据";
            ProgressBarCtrl(0, 0, CommDelayTime / timer.Interval);
        }
        private void btReadAllRealData_Click(object sender, EventArgs e)
        {
            if (false == DeviceStatus(true, true, true))
            {
                return;
            }
            try
            {
                ReadPos = 0;
                ReadAllRealDataProc();
            }
            catch (Exception ex)
            {
                Command = enCmd.指令空闲;
                lbCurState.Text = "设备状态：空闲";
                ProgressBarCtrl(0, 0, 1000);
                MessageBox.Show("读取全部表具定时定量数据时出错：" + ex.Message, "错误");
            }
        }

        #endregion
        #region 全部表具冻结数据
        private void ReadAllFreezeDataProc()
        {
            byte[] dataBuf = new byte[3];
            dataBuf[0] = (byte)ReadPos;
            dataBuf[1] = (byte)(ReadPos >> 8);
            dataBuf[2] = (byte)("无线通信" == cmbComType.Text ? 1 : 6);
            byte[] txBuf = ConstructTxBuffer(enCmd.批量读取集中器中的冻结数据, NeedAck, null, dataBuf);
            DataTransmit(txBuf);
            AddStringToCommBox(false, "\n<<<-----------------读取全部表具冻结数据----------------->>>", null, Color.DarkGreen);
            AddStringToCommBox(true, "发送：", txBuf, Color.Black);
            lbCurState.Text = "设备状态：读取全部表具冻结数据";
            ProgressBarCtrl(0, 0, CommDelayTime / timer.Interval);
        }
        private void btReadAllFreezeData_Click(object sender, EventArgs e)
        {
            if (false == DeviceStatus(true, true, true))
            {
                return;
            }
            try
            {
                ReadPos = 0;
                ReadAllFreezeDataProc();
            }
            catch (Exception ex)
            {
                Command = enCmd.指令空闲;
                lbCurState.Text = "设备状态：空闲";
                ProgressBarCtrl(0, 0, 1000);
                MessageBox.Show("读取全部表具冻结数据时出错：" + ex.Message, "错误");
            }
        }

        #endregion
        #region 保存所有数据到Excel
        [DllImport("User32.dll", CharSet = CharSet.Auto)]
        public static extern int GetWindowThreadProcessId(IntPtr hwnd, out int ID);
        private void btSaveDataToXml_Click(object sender, EventArgs e)
        {
            //            bool bResult;

            //            if (false == DeviceStatus(false, true, true))
            //            {
            //                return;
            //            }
            //            if (Document.Rows.Count == 0)
            //            {
            //                MessageBox.Show("没有数据可以保存！", "错误");
            //                return;
            //            }
            //            string strDirectory = Common.XmlHelper.GetNodeDefValue(strConfigFile, "/Config/Global/ExportToXml", System.Windows.Forms.Application.StartupPath);
            //            saveFileDlg.Filter = "*.xlsx(工作簿)|*.xlsx";
            //            saveFileDlg.DefaultExt = "xlsx";
            //            saveFileDlg.FileName = tbConcAddr.Text + "_" + DateTime.Now.ToString("yyMMddHHmm") + strDataType;
            //            if (saveFileDlg.ShowDialog() == DialogResult.Cancel)
            //            {
            //                return;
            //            }
            //            string strFileName = saveFileDlg.FileName;
            //            if (strFileName.Length == 0)
            //            {
            //                return;
            //            }
            //            if (strDirectory != Path.GetDirectoryName(strFileName))
            //            {
            //                strDirectory = Path.GetDirectoryName(strFileName);
            //                Common.XmlHelper.SetNodeValue(strConfigFile, "/Config/Global", "ExportToXml", strDirectory);
            //            }

            //            Microsoft.Office.Interop.Excel.Application appExcel = new Microsoft.Office.Interop.Excel.Application();
            //            System.Reflection.Missing miss = System.Reflection.Missing.Value;
            //            Microsoft.Office.Interop.Excel.Workbook workbookdata = null;
            //            Microsoft.Office.Interop.Excel.Worksheet worksheetdata = null;
            //            Microsoft.Office.Interop.Excel.Range rangedata;
            //            try
            //            {
            //                if (appExcel == null)
            //                {
            //                    return;
            //                }
            //                //设置对象不可见
            //                appExcel.Visible = false;
            //                appExcel.DisplayAlerts = false;
            //                //System.Globalization.CultureInfo currentci = System.Threading.Thread.CurrentThread.CurrentCulture;
            //                //System.Threading.Thread.CurrentThread.CurrentCulture = new System.Globalization.CultureInfo("en-us");
            //                workbookdata = appExcel.Workbooks.Add(miss);
            //                worksheetdata = (Microsoft.Office.Interop.Excel.Worksheet)workbookdata.Worksheets.Add(miss, miss, miss, miss);
            //                appExcel.ActiveWindow.DisplayGridlines = false;

            //                // 设置字号,文本显示
            //                rangedata = worksheetdata.get_Range("A1", "Z1200");
            //                rangedata.Font.Size = 10;
            //                rangedata.NumberFormatLocal = "@";
            //                rangedata.HorizontalAlignment = XlHAlign.xlHAlignCenter;

            //                // 设置列宽
            //                char cColumn = 'A';
            //                int iColumn = 1;
            //                worksheetdata.get_Range(cColumn++.ToString() + "1").ColumnWidth = 5;              // 序号
            //                worksheetdata.Cells[1, iColumn++] = dgvDoc.Columns["序号"].HeaderText;
            //                worksheetdata.get_Range(cColumn++.ToString() + "1").ColumnWidth = 13.5;           // 地址
            //                worksheetdata.Cells[1, iColumn++] = dgvDoc.Columns["表具地址"].HeaderText;
            //                worksheetdata.get_Range(cColumn++.ToString() + "1").ColumnWidth = 7;              // 类型
            //                worksheetdata.Cells[1, iColumn++] = dgvDoc.Columns["类型"].HeaderText;
            //                worksheetdata.get_Range(cColumn++.ToString() + "1").ColumnWidth = 12;             // 路径0
            //                worksheetdata.Cells[1, iColumn++] = dgvDoc.Columns["路径0"].HeaderText;
            //                worksheetdata.get_Range(cColumn++.ToString() + "1").ColumnWidth = 12;             // 路径1
            //                worksheetdata.Cells[1, iColumn++] = dgvDoc.Columns["路径1"].HeaderText;
            //                worksheetdata.get_Range(cColumn++.ToString() + "1").ColumnWidth = 12;             // 结果
            //                worksheetdata.Cells[1, iColumn++] = dgvDoc.Columns["结果"].HeaderText;
            //                worksheetdata.get_Range(cColumn++.ToString() + "1").ColumnWidth = 8;              // 下行场强
            //                worksheetdata.Cells[1, iColumn++] = dgvDoc.Columns["场强↓"].HeaderText;
            //                worksheetdata.get_Range(cColumn++.ToString() + "1").ColumnWidth = 8;              // 上行场强
            //                worksheetdata.Cells[1, iColumn++] = dgvDoc.Columns["场强↑"].HeaderText;
            //                worksheetdata.get_Range(cColumn++.ToString() + "1").ColumnWidth = 12;             // 报警状态
            //                worksheetdata.Cells[1, iColumn++] = dgvDoc.Columns["报警信息"].HeaderText;
            //                worksheetdata.get_Range(cColumn++.ToString() + "1").ColumnWidth = 8;              // 阀状态
            //                worksheetdata.Cells[1, iColumn++] = dgvDoc.Columns["阀门状态"].HeaderText;
            //                worksheetdata.get_Range(cColumn++.ToString() + "1").ColumnWidth = 5.5;            // 电压
            //                worksheetdata.Cells[1, iColumn++] = dgvDoc.Columns["电压"].HeaderText;
            //                worksheetdata.get_Range(cColumn++.ToString() + "1").ColumnWidth = 5.5;            // 温度
            //                worksheetdata.Cells[1, iColumn++] = dgvDoc.Columns["温度"].HeaderText;
            //                worksheetdata.get_Range(cColumn++.ToString() + "1").ColumnWidth = 5.5;            // 信噪比
            //                worksheetdata.Cells[1, iColumn++] = dgvDoc.Columns["信噪比"].HeaderText;
            //                worksheetdata.get_Range(cColumn++.ToString() + "1").ColumnWidth = 12;             // 信道
            //                worksheetdata.Cells[1, iColumn++] = dgvDoc.Columns["信道"].HeaderText;
            //                worksheetdata.get_Range(cColumn++.ToString() + "1").ColumnWidth = 8;              // 版本
            //                worksheetdata.Cells[1, iColumn++] = dgvDoc.Columns["版本"].HeaderText;
            //                if ("定量数据" == strDataType)
            //                {
            //                    worksheetdata.get_Range(cColumn++.ToString() + "1").ColumnWidth = 16;         // 抄读日期
            //                    worksheetdata.Cells[1, iColumn++] = dgvDoc.Columns["读取时间"].HeaderText;
            //                    worksheetdata.get_Range(cColumn++.ToString() + "1").ColumnWidth = 10;         // 正转用量
            //                    worksheetdata.Cells[1, iColumn++] = dgvDoc.Columns["正转用量"].HeaderText;
            //                    worksheetdata.get_Range(cColumn++.ToString() + "1").ColumnWidth = 10;         // 反转用量
            //                    worksheetdata.Cells[1, iColumn++] = dgvDoc.Columns["反转用量"].HeaderText;
            //                }
            //                else if ("冻结数据0" == strDataType)
            //                {
            //                    worksheetdata.get_Range(cColumn++.ToString() + "1").ColumnWidth = 8;          // 起始序号
            //                    worksheetdata.Cells[1, iColumn++] = dgvDoc.Columns["起始序号"].HeaderText;
            //                    worksheetdata.get_Range(cColumn++.ToString() + "1").ColumnWidth = 16;         // 冻结日期
            //                    worksheetdata.Cells[1, iColumn++] = dgvDoc.Columns["冻结时间"].HeaderText;
            //                    worksheetdata.get_Range(cColumn++.ToString() + "1").ColumnWidth = 8;          // 冻结方式
            //                    worksheetdata.Cells[1, iColumn++] = dgvDoc.Columns["冻结方式"].HeaderText;
            //                    worksheetdata.get_Range(cColumn++.ToString() + "1").ColumnWidth = 8;          // 冻结数量
            //                    worksheetdata.Cells[1, iColumn++] = dgvDoc.Columns["冻结数量"].HeaderText;
            //                    worksheetdata.get_Range(cColumn++.ToString() + "1").ColumnWidth = 10;          // 时间间隔
            //                    worksheetdata.Cells[1, iColumn++] = dgvDoc.Columns["时间间隔"].HeaderText;
            //                    worksheetdata.get_Range(cColumn.ToString() + "1").ColumnWidth = 35;            // 冻结数据
            //                    worksheetdata.Cells[1, iColumn++] = dgvDoc.Columns["冻结数据"].HeaderText;
            //                    worksheetdata.get_Range(cColumn.ToString() + "1").HorizontalAlignment = XlHAlign.xlHAlignLeft;
            ////                    worksheetdata.get_Range(cColumn.ToString() + "1").EntireColumn.AutoFit();
            //                    cColumn++;
            //                }
            //                else if ("冻结数据1" == strDataType)
            //                {
            //                    worksheetdata.get_Range(cColumn++.ToString() + "1").ColumnWidth = 8;          // 起始序号
            //                    worksheetdata.Cells[1, iColumn++] = dgvDoc.Columns["起始序号"].HeaderText;
            //                    worksheetdata.get_Range(cColumn++.ToString() + "1").ColumnWidth = 16;         // 冻结日期
            //                    worksheetdata.Cells[1, iColumn++] = dgvDoc.Columns["冻结时间"].HeaderText;
            //                    worksheetdata.get_Range(cColumn.ToString() + "1").ColumnWidth = 35;           // 冻结数据
            //                    worksheetdata.Cells[1, iColumn++] = dgvDoc.Columns["冻结数据"].HeaderText;
            //                    worksheetdata.get_Range(cColumn.ToString() + "1").HorizontalAlignment = XlHAlign.xlHAlignLeft;
            ////                    worksheetdata.get_Range(cColumn.ToString() + "1").EntireColumn.AutoFit();
            //                    cColumn++;
            //                }
            //                else
            //                {
            //                    worksheetdata.get_Range(cColumn++.ToString() + "1").ColumnWidth = 16;         // 抄读日期
            //                    worksheetdata.Cells[1, iColumn++] = dgvDoc.Columns["读取时间"].HeaderText;
            //                    worksheetdata.get_Range(cColumn++.ToString() + "1").ColumnWidth = 10;         // 正转用量
            //                    worksheetdata.Cells[1, iColumn++] = dgvDoc.Columns["正转用量"].HeaderText;
            //                    worksheetdata.get_Range(cColumn++.ToString() + "1").ColumnWidth = 10;         // 反转用量
            //                    worksheetdata.Cells[1, iColumn++] = dgvDoc.Columns["反转用量"].HeaderText;
            //                    worksheetdata.get_Range(cColumn++.ToString() + "1").ColumnWidth = 8;          // 起始序号
            //                    worksheetdata.Cells[1, iColumn++] = dgvDoc.Columns["起始序号"].HeaderText;
            //                    worksheetdata.get_Range(cColumn++.ToString() + "1").ColumnWidth = 16;         // 冻结日期
            //                    worksheetdata.Cells[1, iColumn++] = dgvDoc.Columns["冻结时间"].HeaderText;
            //                    worksheetdata.get_Range(cColumn++.ToString() + "1").ColumnWidth = 8;          // 冻结方式
            //                    worksheetdata.Cells[1, iColumn++] = dgvDoc.Columns["冻结方式"].HeaderText;
            //                    worksheetdata.get_Range(cColumn++.ToString() + "1").ColumnWidth = 8;          // 冻结数量
            //                    worksheetdata.Cells[1, iColumn++] = dgvDoc.Columns["冻结数量"].HeaderText;
            //                    worksheetdata.get_Range(cColumn++.ToString() + "1").ColumnWidth = 8;          // 时间间隔
            //                    worksheetdata.Cells[1, iColumn++] = dgvDoc.Columns["时间间隔"].HeaderText;
            //                    worksheetdata.get_Range(cColumn.ToString() + "1").ColumnWidth = 35;           // 冻结数据
            //                    worksheetdata.Cells[1, iColumn++] = dgvDoc.Columns["冻结数据"].HeaderText;
            //                    worksheetdata.get_Range(cColumn.ToString() + "1").HorizontalAlignment = XlHAlign.xlHAlignLeft;
            //                    worksheetdata.get_Range(cColumn++.ToString() + "1").EntireColumn.AutoFit();
            //                }

            //                // 冻结
            //                cColumn--;
            //                appExcel.ActiveWindow.SplitRow = 1;
            //                appExcel.ActiveWindow.SplitColumn = 2;
            //                appExcel.ActiveWindow.FreezePanes = true;
            //                // 背景色,不能超过Z，否则按下面方法处理
            //                rangedata = worksheetdata.get_Range("A1", cColumn.ToString() + "1");
            //                rangedata.Font.Bold = true;
            //                rangedata.Interior.ColorIndex = 35;
            //                for (int i = 0; i < Document.Rows.Count; i++ )
            //                {
            //                    if (i %2 == 0)
            //                    {
            //                        continue;
            //                    }
            //                    rangedata = worksheetdata.get_Range("A" + (i + 2).ToString(), cColumn.ToString() + (i + 2).ToString());
            //                    rangedata.Interior.ColorIndex = 19;
            //                }

            //                // 画表格
            //                if (cColumn > 'Z')
            //                {
            //                    rangedata = worksheetdata.get_Range("A1", "A" + ((char)(cColumn - 'Z' + 'A' - 1)).ToString() + (Document.Rows.Count + 1).ToString());
            //                }
            //                else
            //                {
            //                    rangedata = worksheetdata.get_Range("A1", cColumn.ToString() + (Document.Rows.Count + 1).ToString());
            //                }
            //                rangedata.Borders.LineStyle = Microsoft.Office.Interop.Excel.XlLineStyle.xlContinuous;
            //                rangedata.Borders.get_Item(Microsoft.Office.Interop.Excel.XlBordersIndex.xlEdgeTop).Weight = XlBorderWeight.xlMedium;
            //                rangedata.Borders.get_Item(Microsoft.Office.Interop.Excel.XlBordersIndex.xlEdgeBottom).Weight = XlBorderWeight.xlMedium;
            //                rangedata.Borders.get_Item(Microsoft.Office.Interop.Excel.XlBordersIndex.xlEdgeLeft).Weight = XlBorderWeight.xlMedium;
            //                rangedata.Borders.get_Item(Microsoft.Office.Interop.Excel.XlBordersIndex.xlEdgeRight).Weight = XlBorderWeight.xlMedium;

            //                // 填参数数据
            //                //worksheetdata.Cells[1, 1] = "创建时间：";
            //                //worksheetdata.Cells[1, 3] = DateTime.Now.ToString("yyyy/MM/dd HH:mm:ss").ToString();
            //                //worksheetdata.Cells[2, 1] = "集中器地址：";
            //                //worksheetdata.Cells[2, 3] = tbConcAddr.Text;
            //                //worksheetdata.Cells[3, 1] = "版本信息：";
            //                //worksheetdata.Cells[3, 3] = "软件:" + tbSwVer.Text + " 硬件:" + tbHwVer.Text + " 协议:" + tbPtVer.Text;
            //                //worksheetdata.Cells[4, 1] = "工作模式：";
            //                //worksheetdata.Cells[4, 3] = tbWorkMode.Text;

            //                // 填表数据
            //                worksheetdata.Name = tbConcAddr.Text + "_" + strDataType;
            //                //int iColumn = 1;
            //                //for (int i = 0; i < tbDocument.Columns.Count; i++ )
            //                //{
            //                //    if (strDataType == "定量数据")
            //                //    {
            //                //        if (tbDocument.Columns[i].ColumnName == "ColStartNo" ||
            //                //            tbDocument.Columns[i].ColumnName == "ColFreezeTime" ||
            //                //            tbDocument.Columns[i].ColumnName == "ColIncrementVal")
            //                //        {
            //                //            continue;
            //                //        }
            //                //    }
            //                //    else if ("冻结数据" == strDataType)
            //                //    {
            //                //        if (tbDocument.Columns[i].ColumnName == "ColRdTime" ||
            //                //            tbDocument.Columns[i].ColumnName == "ColFwdValue" ||
            //                //            tbDocument.Columns[i].ColumnName == "ColRwdValue")
            //                //        {
            //                //            continue;
            //                //        }
            //                //    }
            //                //    worksheetdata.Cells[6, iColumn++] = dgvDoc.Columns[i].HeaderText.ToString();
            //                //}

            //                //在内存中声明一个数组
            //                object[,] objval = new object[Document.Rows.Count, iColumn];
            //                int iCol = 0;
            //                for (int iRow = 0; iRow < Document.Rows.Count; iRow++ )
            //                {
            //                    iCol = 0;
            //                    DataRow dr = Document.Rows[iRow];
            //                    objval[iRow, iCol++] = iRow.ToString("D4");
            //                    objval[iRow, iCol++] = dr["表具地址"].ToString();
            //                    objval[iRow, iCol++] = dr["类型"].ToString();
            //                    objval[iRow, iCol++] = dr["路径0"].ToString();
            //                    objval[iRow, iCol++] = dr["路径1"].ToString();
            //                    objval[iRow, iCol++] = dr["结果"].ToString();
            //                    objval[iRow, iCol++] = dr["场强↓"].ToString();
            //                    objval[iRow, iCol++] = dr["场强↑"].ToString();
            //                    objval[iRow, iCol++] = dr["报警信息"].ToString();
            //                    objval[iRow, iCol++] = dr["阀门状态"].ToString();
            //                    objval[iRow, iCol++] = dr["电压"].ToString();
            //                    objval[iRow, iCol++] = dr["温度"].ToString();
            //                    objval[iRow, iCol++] = dr["信噪比"].ToString();
            //                    objval[iRow, iCol++] = dr["信道"].ToString();
            //                    objval[iRow, iCol++] = dr["版本"].ToString();
            //                    if ("定量数据" == strDataType)
            //                    {
            //                        objval[iRow, iCol++] = dr["读取时间"].ToString();
            //                        objval[iRow, iCol++] = dr["正转用量"].ToString();
            //                        objval[iRow, iCol++] = dr["反转用量"].ToString();
            //                    }
            //                    else if ("冻结数据0" == strDataType)
            //                    {
            //                        objval[iRow, iCol++] = dr["起始序号"].ToString();
            //                        objval[iRow, iCol++] = dr["冻结时间"].ToString();
            //                        objval[iRow, iCol++] = dr["冻结方式"].ToString();
            //                        objval[iRow, iCol++] = dr["冻结数量"].ToString();
            //                        objval[iRow, iCol++] = dr["时间间隔"].ToString();
            //                        objval[iRow, iCol++] = dr["冻结数据"].ToString();
            //                    }
            //                    else if ("冻结数据1" == strDataType)
            //                    {
            //                        objval[iRow, iCol++] = dr["起始序号"].ToString();
            //                        objval[iRow, iCol++] = dr["冻结时间"].ToString();
            //                        objval[iRow, iCol++] = dr["冻结数据"].ToString();
            //                    }
            //                    else
            //                    {
            //                        objval[iRow, iCol++] = dr["读取时间"].ToString();
            //                        objval[iRow, iCol++] = dr["正转用量"].ToString();
            //                        objval[iRow, iCol++] = dr["反转用量"].ToString();
            //                        objval[iRow, iCol++] = dr["起始序号"].ToString();
            //                        objval[iRow, iCol++] = dr["冻结时间"].ToString();
            //                        objval[iRow, iCol++] = dr["冻结方式"].ToString();
            //                        objval[iRow, iCol++] = dr["冻结数量"].ToString();
            //                        objval[iRow, iCol++] = dr["时间间隔"].ToString();
            //                        objval[iRow, iCol++] = dr["冻结数据"].ToString();
            //                    }

            //                    System.Windows.Forms.Application.DoEvents();
            //                }
            //                if ('A' + iCol - 1 > 'Z')
            //                {
            //                    rangedata = worksheetdata.get_Range("A2", "A" + ((char)('A' + iCol - 1 - 'Z' + 'A' - 1)).ToString() + (dgvDoc.Rows.Count + 1).ToString());
            //                }
            //                else
            //                {
            //                    rangedata = worksheetdata.get_Range("A2", ((char)('A' + iCol - 1)).ToString() + (Document.Rows.Count + 1).ToString());
            //                }
            //                rangedata.Value2 = objval;

            //                //保存工作表
            //                System.Runtime.InteropServices.Marshal.ReleaseComObject(rangedata);
            //                rangedata = null;

            //                //调用方法关闭excel进程
            //                //appExcel.Visible = true;
            //                appExcel.ActiveWorkbook.SaveAs(strFileName);
            //                //appExcel.ActiveWorkbook.SaveAs(strFileName, Microsoft.Office.Interop.Excel.XlFileFormat.xlWorkbookNormal);
            //                //xlbook.saveas(filepath, microsoft.office.interop.excel .xlfileformat.xlexcel 8,        type.missing, type.missing, type.missing, type.missing, excel .xlsaveasaccessmode.xlnochange, type.missing, type.missing, type.missing, type.missing, type.missing);
            //                //xlbook.saveas(filepath, microsoft.office.interop.excel .xlfileformat.xlworkbooknormal, type.missing, type.missing, type.missing, type.missing, excel .xlsaveasaccessmode.xlnochange, type.missing, type.missing, type.missing, type.missing, type.missing);
            //                appExcel.SaveWorkspace();
            //                bResult = true;
            //            }
            //            catch (Exception ex)
            //            {
            //                MessageBox.Show("导出数据到文件发生错误，" + ex.Message, "出错了", MessageBoxButtons.OK, MessageBoxIcon.Information);
            //                bResult = false;
            //            }
            //            finally
            //            {
            //                workbookdata.Close(false, miss, miss);
            //                appExcel.Workbooks.Close();
            //                appExcel.Quit();
            //                IntPtr t = new IntPtr(appExcel.Hwnd);          //杀死进程的好方法，很有效
            //                int k = 0;
            //                GetWindowThreadProcessId(t, out k);
            //                System.Diagnostics.Process p = System.Diagnostics.Process.GetProcessById(k);
            //                p.Kill();
            //            }
            //            if (false == bResult)
            //            {
            //                return;
            //            }
            //            if (DialogResult.Yes == MessageBox.Show("导出文件成功！是否打开这个文件？", "导出成功", MessageBoxButtons.YesNo, MessageBoxIcon.Question))
            //            {
            //                Process.Start(strFileName);
            //            }
        }
        #endregion
        #region 集中器数据转发功能
        private byte[] TxByte = new byte[200];
        private byte GetCmdFromString(string strCommand)
        {
            switch (strCommand)
            {
                case "读取表具计量信息": return 0x01;
                case "表具阀门控制": return 0x03;
                case "设置表端功能使能状态": return 0x08;
                case "读取表端功能使能状态": return 0x0B;
                default: return 0xFF;
            }
        }
        public static string GetStringFromCmd(byte bCmd)
        {
            switch (bCmd)
            {
                case 0x01: return "读取表具计量信息";
                case 0x03: return "开关阀指令";
                case 0x08: return "设置表端功能使能状态";
                case 0x0B: return "读取表端功能使能状态";
                default: return "未知指令";
            }
        }
        private void cbRunNodeAddr_KeyPress(object sender, KeyPressEventArgs e)
        {
            ComboBox tbAddress = (ComboBox)sender;

            if ("0123456789\b\r\x03\x16\x18".IndexOf(e.KeyChar) < 0)
            {
                e.Handled = true;
                return;
            }
            if (e.KeyChar == '\r')
            {
                tbAddress.Text = tbAddress.Text.PadLeft(AddrLength * 2, '0');
                int iLoop = 0;
                for (; strDataFwdNodeAddrList != null && iLoop < strDataFwdNodeAddrList.Length; iLoop++)
                {
                    if (tbAddress.Text == strDataFwdNodeAddrList[iLoop])
                    {
                        break;
                    }
                }
                if (strDataFwdNodeAddrList == null)
                {
                    strDataFwdNodeAddrList = new string[1];
                    strDataFwdNodeAddrList[0] = cbRunNodeAddr.Text;
                }
                if (iLoop >= strDataFwdNodeAddrList.Length)
                {
                    Array.Resize(ref strDataFwdNodeAddrList, strDataFwdNodeAddrList.Length + 1);
                    strDataFwdNodeAddrList[strDataFwdNodeAddrList.Length - 1] = tbAddress.Text;
                }
                e.Handled = true;
            }
            if (tbAddress.Text.Length >= AddrLength * 2 && e.KeyChar != '\b')
            {
                if (tbAddress.SelectionLength == 0)
                {
                    e.Handled = true;
                }
            }
        }
        private void btClearNodeAddr_Click(object sender, EventArgs e)
        {
            strDataFwdNodeAddrList = null;
            cbRunNodeAddr.Text = "";
            iLastDataFwdNodeCount = 0;
        }
        private void cbRunNodeAddr_Leave(object sender, EventArgs e)
        {
            //if (cbRunNodeAddr.Text != "")
            //{
            //    cbRunNodeAddr.Text = cbRunNodeAddr.Text.PadLeft(AddrLength * 2, '0');
            //    int iLoop = 0;
            //    for (; strDataFwdNodeAddrList != null && iLoop < strDataFwdNodeAddrList.Length; iLoop++)
            //    {
            //        if (cbRunNodeAddr.Text == strDataFwdNodeAddrList[iLoop])
            //        {
            //            break;
            //        }
            //    }
            //    if (strDataFwdNodeAddrList == null)
            //    {
            //        strDataFwdNodeAddrList = new string[1];
            //        strDataFwdNodeAddrList[0] = cbRunNodeAddr.Text;
            //    }
            //    if (iLoop >= strDataFwdNodeAddrList.Length)
            //    {
            //        Array.Resize(ref strDataFwdNodeAddrList, strDataFwdNodeAddrList.Length + 1);
            //        strDataFwdNodeAddrList[strDataFwdNodeAddrList.Length - 1] = cbRunNodeAddr.Text;
            //    }
            //}
        }

        private void btRunCmd_Click(object sender, EventArgs e)
        {
            if (btRunCmd.Text == "执行命令")
            {
                if (false == DeviceStatus(true, true, true))
                {
                    return;
                }
                if (cmbOperateCmd.SelectedIndex == 0)
                {
                    MessageBox.Show("请选择要执行的命令！", "命令选择", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }

                if (strDataFwdNodeAddrList == null || strDataFwdNodeAddrList.Length == 0)
                {
                    MessageBox.Show("任务队列为空，请输入或选择要执行任务的表具地址", "任务列表空");
                    return;
                }
                if (strDataFwdNodeAddrList != null && strDataFwdNodeAddrList.Length > 0)
                {
                    strCurDataFwdAddr = strDataFwdNodeAddrList[0];
                }

                if (strCurDataFwdAddr.Length != AddrLength * 2)
                {
                    strDataFwdNodeAddrList = null;
                    MessageBox.Show("当前表具地址中的数据不合法！", "地址错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    strCurDataFwdAddr = "";
                    return;
                }
                btRunCmd.Text = "停止命令";
                gpbFunList.Enabled = false;
                cmbOperateCmd.Enabled = false;
                cbRunNodeAddr.Enabled = false;
                try
                {
                    byte[] dataBuf = new byte[200];
                    int iLen = 0;
                    dataBuf[iLen++] = GetCmdFromString(cmbOperateCmd.Text);
                    iLen += GetByteAddrFromString(strCurDataFwdAddr, dataBuf, iLen);
                    foreach (Control ctr in gpbFunList.Controls)
                    {
                        if (ctr is IGetParas) iLen += (ctr as IGetParas).GetDataBuf(dataBuf, iLen);
                    }
                    Array.Resize(ref dataBuf, iLen);
                    Array.Resize(ref TxByte, iLen);
                    Array.Copy(dataBuf, TxByte, iLen);
                    byte[] txBuf = ConstructTxBuffer(enCmd.集中器数据转发, NeedAck, null, dataBuf);
                    DataTransmit(txBuf);
                    AddStringToCommBox(false, "\n<<<-----------------集中器数据转发：" + cmbOperateCmd.Text + "----------------->>>", null, Color.DarkGreen, true);
                    AddStringToCommBox(true, "发送：", txBuf, Color.Black);
                    lbCurState.Text = "设备状态：集中器数据转发-" + cmbOperateCmd.Text;
                    ProgressBarCtrl(0, 0, 80 * 1000 / timer.Interval);
                }
                catch (Exception ex)
                {
                    Command = enCmd.指令空闲;
                    lbCurState.Text = "设备状态：空闲";
                    ProgressBarCtrl(0, 0, 1000);
                    btRunCmd.Text = "执行命令";
                    gpbFunList.Enabled = true;
                    cmbOperateCmd.Enabled = true;
                    cbRunNodeAddr.Enabled = true;
                    strDataFwdNodeAddrList = null;
                    strCurDataFwdAddr = "";
                    MessageBox.Show("集中器数据转发出错：" + ex.Message, "错误");
                }
            }
            else
            {
                if (DialogResult.Cancel == MessageBox.Show("集中器需要最长为3分钟才能停止进程，请确认是否停止指令！", "停止确认", MessageBoxButtons.OKCancel, MessageBoxIcon.Question))
                {
                    return;
                }
                Command = enCmd.指令空闲;
                lbCurState.Text = "设备状态：空闲";
                ProgressBarCtrl(0, 0, 1000);
                btRunCmd.Text = "执行命令";
                gpbFunList.Enabled = true;
                cmbOperateCmd.Enabled = true;
                cbRunNodeAddr.Enabled = true;
                cbRunNodeAddr.Text = "";
                lbNodeAddrList.Text = "表具地址列表[0]";
                strDataFwdNodeAddrList = null;
                strCurDataFwdAddr = "";
            }
        }

        #endregion
        #region 其他公共功能函数
        public static string[] ExplainMeterData(byte[] DataBuf, int iStart, int iLength)
        {
            int index;
            string[] strResult = new string[20];

            try
            {
                for (index = 0; index < strResult.Length; index++)
                {
                    strResult[index] = "";
                }

                index = 0;
                // 实时数据
                if (iLength == 22)
                {
                    // 正转用量+反转用量
                    strResult[index++] = Byte6ToUint64(DataBuf, iStart) + "m³";
                    iStart += 6;
                    strResult[index++] = Byte6ToUint64(DataBuf, iStart) + "m³";
                    iStart += 6;
                }
                // 冻结数据(旧格式)
                else if (iLength == 89)
                {
                    // 起始序号+冻结时间+冻结方式+冻结数量+时间间隔+冻结数据
                    strResult[index++] = DataBuf[iStart++].ToString("D");
                    strResult[index++] = DataBuf[iStart++].ToString("X2") + DataBuf[iStart++].ToString("X2") + "年" +
                                         DataBuf[iStart++].ToString("X2") + "月" + DataBuf[iStart++].ToString("X2") + "日" + DataBuf[iStart++].ToString("X2") + "时";
                    strResult[index++] = (DataBuf[iStart++] & 0x80) == 0x80 ? "按月冻结" : "按日冻结";
                    strResult[index++] = DataBuf[iStart++].ToString("D");
                    if (DataBuf[iStart] == 0)
                    {
                        strResult[index] = "1条/日(月)";
                    }
                    else
                    {
                        strResult[index] = DataBuf[iStart].ToString("D") + "时/日(月)";
                    }
                    index++;
                    iStart++;
                    strResult[index] = "";
                    for (int iLoop = 0; iLoop < 10; iLoop++)
                    {
                        strResult[index] += Byte6ToUint64(DataBuf, iStart) + "m³";
                        iStart += 6;
                        strResult[index] += "/" + DataBuf[iStart++].ToString("D") + "日→";
                    }
                    strResult[index] = strResult[index].TrimEnd('→');
                    index++;
                }
                // 冻结数据(新格式)
                else if (iLength - iStart == 115)
                {
                    // 起始序号+冻结时间+冻结数据
                    strResult[index++] = DataBuf[iStart++].ToString("D");
                    strResult[index++] = DataBuf[iStart++].ToString("X2") + "月" + DataBuf[iStart++].ToString("X2") + "日" +
                                         DataBuf[iStart++].ToString("X2") + "时" + DataBuf[iStart++].ToString("X2") + "分";

                    int iBase = 0;
                    for (int iLoop = 0; iLoop < 4; iLoop++)
                    {
                        iBase <<= 8;
                        iBase += DataBuf[iStart + 4 - iLoop - 1];
                    }
                    iBase *= 1000;
                    iBase += DataBuf[iStart + 4] + DataBuf[iStart + 5] * 256;
                    iStart += 6;
                    strResult[index] = (iBase / 1000).ToString("D") + "." + (iBase % 1000).ToString("D3") + "m³→";
                    for (int iLoop = 0; iLoop < 47; iLoop++)
                    {
                        iBase += DataBuf[iStart++] + DataBuf[iStart++] * 256;
                        strResult[index] += (iBase / 1000).ToString("D") + "." + (iBase % 1000).ToString("D3") + "m³→";
                    }
                    strResult[index++] = strResult[index].ToString().TrimEnd('→');
                }
                // 报警
                strResult[index] = "";
                if ((DataBuf[iStart] & 0x01) == 0x01)
                {
                    strResult[index] += "干簧管故障、";
                }
                if ((DataBuf[iStart] & 0x02) == 0x02)
                {
                    strResult[index] += "阀门故障、";
                }
                if ((DataBuf[iStart] & 0x04) == 0x04)
                {
                    strResult[index] += "传感器线断开、";
                }
                if ((DataBuf[iStart] & 0x08) == 0x08)
                {
                    strResult[index] += "电池欠压、";
                }
                if ((DataBuf[iStart] & 0x20) == 0x20)
                {
                    strResult[index] += "磁干扰、";
                }
                if ((DataBuf[iStart] & 0x40) == 0x40)
                {
                    strResult[index] += "光电直读表坏、";
                }
                if ((DataBuf[iStart] & 0x80) == 0x80)
                {
                    strResult[index] += "光电直读表被强光干扰、";
                }
                iStart += 1;
                if ((DataBuf[iStart] & 0x01) == 0x01)
                {
                    strResult[index] += "水表反转、";
                }
                if ((DataBuf[iStart] & 0x02) == 0x02)
                {
                    strResult[index] += "水表被拆卸、";
                }
                if ((DataBuf[iStart] & 0x04) == 0x04)
                {
                    strResult[index] += "水表被垂直安装、";
                }
                if ((DataBuf[iStart] & 0x08) == 0x08)
                {
                    strResult[index] += "Eeprom异常、";
                }
                if ((DataBuf[iStart] & 0x10) == 0x10)
                {
                    strResult[index] += "煤气泄漏、";
                }
                if ((DataBuf[iStart] & 0x20) == 0x20)
                {
                    strResult[index] += "欠费标志、";
                }
                if (strResult[index].ToString() == "")
                {
                    strResult[index] = "无报警内容";
                }
                else
                {
                    strResult[index] = strResult[index].ToString().TrimEnd('、');
                }
                iStart += 1;
                index += 1;
                // 阀状态
                if ((DataBuf[iStart] & 0x03) == 0)
                {
                    strResult[index] = "阀门故障";
                }
                else if ((DataBuf[iStart] & 0x03) == 1)
                {
                    strResult[index] = "开阀";
                }
                else if ((DataBuf[iStart] & 0x03) == 2)
                {
                    strResult[index] = "关阀";
                }
                else
                {
                    strResult[index] = "阀门未知";
                }
                iStart += 1;
                index += 1;
                // 电池电压
                if (DataBuf[iStart] >= 0xF0)
                {
                    strResult[index] = "备用电池";
                }
                else
                {
                    strResult[index] = (DataBuf[iStart] / 10).ToString("D") + "." + (DataBuf[iStart] % 10).ToString("D") + "V";
                }
                iStart += 1;
                index += 1;
                // 环境温度
                strResult[index++] = DataBuf[iStart++].ToString("D") + "℃";
                // 信噪比
                if ((DataBuf[iStart] & 0x80) == 0x80)
                {
                    strResult[index] = "-" + (DataBuf[iStart] & 0x7F).ToString("D") + "dB";
                }
                else
                {
                    strResult[index] = "+" + DataBuf[iStart].ToString("D") + "dB";
                }
                // 信道
                iStart += 1;
                index += 1;
                strResult[index++] = "Rx:" + (DataBuf[iStart] >> 4 & 0x0F).ToString("D") + "/Tx:" + (DataBuf[iStart] & 0x0F).ToString("D");
                iStart += 1;
                // 版本
                strResult[index++] = "Ver" + DataBuf[iStart++].ToString("D");
                // 下行信号强度
                strResult[index++] = "-" + DataBuf[iStart++].ToString("D") + "dBm";
                // 上行信号强度
                strResult[index++] = "-" + DataBuf[iStart++].ToString("D") + "dBm";
                Array.Resize(ref strResult, index);
                return strResult;
            }
            catch (Exception ex)
            {
                MessageBox.Show("数据解析出错，" + ex.Message);
                return null;
            }
        }
        private void tsmiClearDocument_Click(object sender, EventArgs e)
        {
            if (Document.Rows.Count > 0)
            {
                if (DialogResult.Yes == MessageBox.Show("是否要清空所有表具档案吗？", "清空确认", MessageBoxButtons.YesNo, MessageBoxIcon.Question))
                {
                    Document.Clear();
                }
            }
        }

        private void cmsMenu_Opening(object sender, CancelEventArgs e)
        {
            if (Document.Rows.Count > 0)
            {
                tsmiClearDocument.Enabled = true;
                tsmiAddToForwardList.Enabled = true;
            }
            else
            {
                tsmiClearDocument.Enabled = false;
                tsmiAddToForwardList.Enabled = false;
            }
        }
        private void tbCurAddr_KeyPress(object sender, KeyPressEventArgs e)
        {
            tbAddress_KeyPress(sender, e, AddrLength * 2, "0123456789\b\r\x03\x16\x18");
        }
        private void tbCurAddr_Leave(object sender, EventArgs e)
        {
            if (tbCurAddr.Text != "")
            {
                tbCurAddr.Text = tbCurAddr.Text.PadLeft(AddrLength * 2, '0');
            }

        }
        #endregion

        #endregion


        private void tbPageRecord_SelectedIndexChanged(object sender, EventArgs e)
        {

        }
        


        private void btReadGprsParam_Click_1(object sender, EventArgs e)
        {
            if (false == DeviceStatus(true, true, true))
            {
                return;
            }
            byte[] txBuf = ConstructTxBuffer(enCmd.读Gprs参数, NeedAck, null, null);
            DataTransmit(txBuf);
            AddStringToCommBox(false, "\n<<<-----------------读取GPRS参数----------------->>>", null, Color.DarkGreen, true);
            AddStringToCommBox(true, "发送：", txBuf, Color.Black);
            lbCurState.Text = "设备状态：读取GPRS参数";
            ProgressBarCtrl(0, 0, CommDelayTime / timer.Interval);
        }

        private void btSetGprsParam_Click_1(object sender, EventArgs e)
        {
            if (false == DeviceStatus(true, true, true))
            {
                return;
            }
            if (DialogResult.Cancel == MessageBox.Show("确认要写入这些GPRS参数吗？", "确认信息", MessageBoxButtons.OKCancel, MessageBoxIcon.Question))
            {
                return;
            }
            try
            {
                byte[] dataBuf = new byte[100];
                int iLen = 0;
                dataBuf[iLen++] = Convert.ToByte(tbServerIp00.Text);
                dataBuf[iLen++] = Convert.ToByte(tbServerIp01.Text);
                dataBuf[iLen++] = Convert.ToByte(tbServerIp02.Text);
                dataBuf[iLen++] = Convert.ToByte(tbServerIp03.Text);
                dataBuf[iLen++] = (byte)Convert.ToUInt16(tbServerPort0.Text);
                dataBuf[iLen++] = (byte)(Convert.ToUInt16(tbServerPort0.Text) >> 8);
                dataBuf[iLen++] = Convert.ToByte(tbServerIp10.Text);
                dataBuf[iLen++] = Convert.ToByte(tbServerIp11.Text);
                dataBuf[iLen++] = Convert.ToByte(tbServerIp12.Text);
                dataBuf[iLen++] = Convert.ToByte(tbServerIp13.Text);
                dataBuf[iLen++] = (byte)Convert.ToUInt16(tbServerPort1.Text);
                dataBuf[iLen++] = (byte)(Convert.ToUInt16(tbServerPort1.Text) >> 8);
                dataBuf[iLen++] = (byte)(nudHeatBeat.Value * 6);
                byte[] apn = System.Text.Encoding.Default.GetBytes(tbApn.Text);
                dataBuf[iLen++] = (byte)apn.Length;
                Array.Copy(apn, 0, dataBuf, iLen, apn.Length);
                iLen += apn.Length;
                byte[] username = System.Text.Encoding.Default.GetBytes(tbUsername.Text);
                dataBuf[iLen++] = (byte)username.Length;
                Array.Copy(username, 0, dataBuf, iLen, username.Length);
                iLen += username.Length;
                byte[] password = System.Text.Encoding.Default.GetBytes(tbPassword.Text);
                dataBuf[iLen++] = (byte)password.Length;
                Array.Copy(password, 0, dataBuf, iLen, password.Length);
                iLen += password.Length;
                Array.Resize(ref dataBuf, iLen);
                byte[] txBuf = ConstructTxBuffer(enCmd.写Gprs参数, NeedAck, null, dataBuf);
                DataTransmit(txBuf);
                AddStringToCommBox(false, "\n<<<-----------------设置GPRS参数----------------->>>", null, Color.DarkGreen, true);
                AddStringToCommBox(true, "发送：", txBuf, Color.Black);
                lbCurState.Text = "设备状态：设置GPRS参数";
                ProgressBarCtrl(0, 0, CommDelayTime / timer.Interval);
            }
            catch (Exception ex)
            {
                Command = enCmd.指令空闲;
                lbCurState.Text = "设备状态：空闲";
                ProgressBarCtrl(0, 0, 1000);
                MessageBox.Show("设置GPRS参数出错：" + ex.Message, "错误");
            }
        }
        
        private void scon3_Panel2_Paint(object sender, PaintEventArgs e)
        {

        }

        private void rtbCommMsg_TextChanged_1(object sender, EventArgs e)
        {

        }

        private void rtbGprsMsg_TextChanged_1(object sender, EventArgs e)
        {

        }

        private void gprsToolStripMenuItem_Click(object sender, EventArgs e)
        {
            rtbGprsMsg.Clear();
        }


        #region 设置SIM卡号

        private void button6_Click(object sender, EventArgs e)
        {
            if (false == DeviceStatus(true, true, true))
            {
                return;
            }
            try
            {
                //只能输入11位或者13位数字
                if ((SimNum.Text.Length != AddrLength * 2 - 1) && (SimNum.Text.Length != AddrLength * 2 + 1))
                {
                    MessageBox.Show("请输入11位或者13位SIM卡号后再试！", "错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }

                //如果输入11位卡号，则在后面增加两位空格符号
                if (SimNum.Text.Length == AddrLength * 2 - 1)
                {
                    SimNum.Text = SimNum.Text.Insert(11,"  ");
                }
                
                //byte[] dataBuf = new byte[13];
                byte[] dataBuf = System.Text.Encoding.ASCII.GetBytes(SimNum.Text);
                
                byte[] txBuf = ConstructTxBuffer(enCmd.设置SIM卡号, NeedAck, null, dataBuf);
                DataTransmit(txBuf);
                AddStringToCommBox(false, "\n<<<-----------------设置 SIM 卡号 ----------------->>>", null, Color.DarkGreen, true);
                AddStringToCommBox(true, "发送：", txBuf, Color.Black);
                lbCurState.Text = "设备状态：设置SIM卡号";
                ProgressBarCtrl(0, 0, CommDelayTime / timer.Interval);
            }
            catch (Exception ex)
            {
                Command = enCmd.指令空闲;
                lbCurState.Text = "设备状态：空闲";
                ProgressBarCtrl(0, 0, 1000);
                MessageBox.Show("设置SIM卡号出错：" + ex.Message, "错误");
            }
        }

        private void ExplainSetSimNum(ProtocolStruct rxStruct)
        {
            try
            {
                if (rxStruct.CommandIDByte == (byte)enCmd.设置SIM卡号)
                {
                    if (rxStruct.DataBuf[0] == 0xAA)
                    {
                        AddStringToCommBox(false, "\n执行结果：成功 ", null, Color.DarkBlue);
                    }
                    else
                    {
                        AddStringToCommBox(false, "\n执行结果：失败  失败原因：" + GetErrorInfo(rxStruct.DataBuf[0]), null, Color.Red);
                    }
                    Command = enCmd.指令空闲;
                    lbCurState.Text = "设备状态：空闲";
                    ProgressBarCtrl(0, 0, 1000);
                }
            }
            catch (Exception ex)
            {
                Command = enCmd.指令空闲;
                lbCurState.Text = "设备状态：空闲";
                ProgressBarCtrl(0, 0, 1000);
            }
            return;
        }
        private void SIM_KeyPress(object sender, KeyPressEventArgs e)
        {
            //限制只能输入数字和回格
            if (!(char.IsNumber(e.KeyChar)) && e.KeyChar != (char)8)
            {
                e.Handled = true;
            }
        }


        #endregion

        #region 设置集中器信道获取方式
        private void btSetConcChannel_Click(object sender, EventArgs e)
        {
            if (false == DeviceStatus(true, true, true))
            {
                return;
            }
            if (DialogResult.Cancel == MessageBox.Show("请确认是否要修改集中器的信道获取方式？", "修改确认", MessageBoxButtons.OKCancel, MessageBoxIcon.Question))
            {
                return;
            }
            byte[] dataBuf = new byte[2];
            if (channelSoftSet.Checked == true)
            {
                dataBuf[0] = 0x00;
                dataBuf[1] = (byte)NumChannel.Value;
            }
            else if (channelServerSet.Checked == true)
            {
                dataBuf[0] = 0x01;
                dataBuf[1] = 0x3;//从服务器获取信道号，默认先设置信号为 3信道
                NumChannel.Value = Bcd2Hex(dataBuf[1]);
            }
            else
            {
                MessageBox.Show("信道获取方式设置有错误，请重新设置！");
                return;
            }
            byte[] txBuf = ConstructTxBuffer(enCmd.写集中器信道设置方式, NeedAck, null, dataBuf);
            DataTransmit(txBuf);
            AddStringToCommBox(false, "\n<<<-----------------写集中器信道设置方式----------------->>>", null, Color.DarkGreen, true);
            AddStringToCommBox(true, "发送：", txBuf, Color.Black);
            lbCurState.Text = "设备状态：写集中器信道设置方式";
            ProgressBarCtrl(0, 0, CommDelayTime / timer.Interval);
        }
        private void ExplainWriteChannelParam(ProtocolStruct rxStruct)
        {
            try
            {
                if (rxStruct.CommandIDByte == (byte)enCmd.写集中器信道设置方式)
                {
                    if (rxStruct.DataBuf[0] == 0xAA)
                    {
                        AddStringToCommBox(false, "\n执行结果：成功", null, Color.DarkBlue);
                    }
                    else
                    {
                        AddStringToCommBox(false, "\n执行结果：失败", null, Color.Red);

                    }
                    lbCurState.Text = "设备状态：空闲";
                    Command = enCmd.指令空闲;
                    ProgressBarCtrl(0, 0, 1000);
                }
            }
            catch (Exception ex)
            {
                Command = enCmd.指令空闲;
                lbCurState.Text = "设备状态：空闲";
                ProgressBarCtrl(0, 0, 1000);
                MessageBox.Show("写集中器信道设置方式出错：" + ex.Message);
            }
            return;
        }
        #endregion
        #region  //读取集中器信道获取方式
        private void btGetConcChannel_Click(object sender, EventArgs e)
        {
            if (false == DeviceStatus(true, true, true))
            {
                return;
            }
            byte[] txBuf = ConstructTxBuffer(enCmd.读集中器信道设置方式, NeedAck, null, null);
            DataTransmit(txBuf);
            AddStringToCommBox(false, "\n<<<-----------------读集中器信道设置方式----------------->>>", null, Color.DarkGreen, true);
            AddStringToCommBox(true, "发送：", txBuf, Color.Black);
            lbCurState.Text = "设备状态：读集中器信道设置方式";
            ProgressBarCtrl(0, 0, CommDelayTime / timer.Interval);
        }
        private void ExplainReadChannelParam(ProtocolStruct rxStruct)
        {
            try
            {
                if (rxStruct.CommandIDByte == (byte)enCmd.读集中器信道设置方式)
                {
                    AddStringToCommBox(false, "\n执行结果：成功", null, Color.DarkBlue);
                    if (rxStruct.DataBuf[0] == 0)//从上位机设置信道号
                    {
                        channelSoftSet.Checked = true;
                        AddStringToCommBox(false, "\n  从上位机设置信道号", null, Color.DarkBlue);
                    }
                    else if (rxStruct.DataBuf[0] == 1)//从服务器获取信道号
                    {
                        channelServerSet.Checked = true;
                        AddStringToCommBox(false, "\n  从服务器获取信道号", null, Color.DarkBlue);
                    }
                    else
                    {
                        channelSoftSet.Checked = false;
                        channelServerSet.Checked = false;
                        AddStringToCommBox(false, "\n  错误的获取信道号方式", null, Color.Red);
                    }

                    NumChannel.Value = rxStruct.DataBuf[1];
                    AddStringToCommBox(false, "\n  信道号：" + NumChannel.Value.ToString(), null, Color.DarkBlue);


                    Command = enCmd.指令空闲;
                    lbCurState.Text = "设备状态：空闲";
                    ProgressBarCtrl(0, 0, 1000);
                }
            }
            catch (Exception ex)
            {
                Command = enCmd.指令空闲;
                lbCurState.Text = "设备状态：空闲";
                ProgressBarCtrl(0, 0, 1000);
                MessageBox.Show("读集中器信道设置方式出错：" + ex.Message);
            }
            return;
        }
        #endregion
        #region 获取SIM卡号
        private void btReadSIM_Click(object sender, EventArgs e)
        {
            if (false == DeviceStatus(true, true, false))
            {
                return;
            }
            byte[] txBuf = ConstructTxBuffer(enCmd.获取SIM卡号, NeedAck, null, null);
            DataTransmit(txBuf);
            AddStringToCommBox(false, "\n<<<-----------------获取SIM卡号----------------->>>", null, Color.DarkGreen, true);
            AddStringToCommBox(true, "发送：", txBuf, Color.Black);
            lbCurState.Text = "设备状态：获取SIM卡号";
            ProgressBarCtrl(0, 0, CommDelayTime / timer.Interval);
        }
        private void ExplainGetSimNum(ProtocolStruct rxStruct)
        {
            try
            {
                if (rxStruct.CommandIDByte == (byte)enCmd.获取SIM卡号)
                {
                    SimNum.Text = System.Text.Encoding.Default.GetString(rxStruct.DataBuf);
                    AddStringToCommBox(false, "\n执行结果：成功  SIM卡号为：" + SimNum.Text, null, Color.DarkBlue);

                    Command = enCmd.指令空闲;
                    lbCurState.Text = "设备状态：空闲";
                    ProgressBarCtrl(0, 0, 1000);
                }
            }
            catch (Exception ex)
            {
                Command = enCmd.指令空闲;
                lbCurState.Text = "设备状态：空闲";
                ProgressBarCtrl(0, 0, 1000);
                MessageBox.Show("获取SIM卡号出错：" + ex.Message);
            }
            return;
        }
        #endregion
    }

    interface IGetParas
    {
        int GetDataBuf(byte[] dataBuf, int iLen);
        string GetResultString(byte[] DataBuf);
    }
    interface IGetResult
    {
        string GetResultString(byte[] DataBuf, byte[] TxBuf);
    }
}
