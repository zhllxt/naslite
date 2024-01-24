using LibreHardwareMonitor.Hardware;
using System.Diagnostics;
using System.Net;
using System.Text;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using Mono.Unix;
using LibreHardwareMonitor.Hardware.Cpu;

namespace Nas
{
    public class TemperatureInfo
    {
        [Newtonsoft.Json.JsonIgnore]
        public Identifier? id;
        public string Type = "";
        public string Name = "";
        public float? Val;
        public float? Max;
    }

    public class UpdateVisitor : IVisitor
    {
        public void VisitComputer(IComputer computer)
        {
            computer.Traverse(this);
        }
        public void VisitHardware(IHardware hardware)
        {
            hardware.Update();
            foreach (IHardware subHardware in hardware.SubHardware) subHardware.Accept(this);
        }
        public void VisitSensor(ISensor sensor) { }
        public void VisitParameter(IParameter parameter) { }
    }

    public class HardwareMonitor
    {
        public Computer? computer;

        public void Open()
        {
            computer = new Computer
            {
                IsBatteryEnabled = false,
                IsCpuEnabled = true,
                IsGpuEnabled = true,
                IsMemoryEnabled = false,
                IsMotherboardEnabled = false,
                IsControllerEnabled = false,
                IsNetworkEnabled = false,
                IsPsuEnabled = false,
                IsStorageEnabled = true
            };

            computer.Open();
            computer.Accept(new UpdateVisitor());
        }

        public void Close()
        {
            computer?.Close();
        }

        public List<TemperatureInfo> GetTemperatures()
        {
            List<TemperatureInfo> Temps = new List<TemperatureInfo>();

            if (computer == null)
                return Temps;

            var addToList = (IHardware hardware, ISensor sensor, string type) =>
            {
                bool isExists = false;
                foreach (TemperatureInfo t in Temps)
                {
                    if (t.id == hardware.Identifier)
                    {
                        isExists = true;
                        break;
                    }
                }
                if (!isExists)
                {
                    TemperatureInfo t = new TemperatureInfo();
                    t.id = hardware.Identifier;
                    t.Type = type;
                    t.Name = hardware.Name;
                    t.Val = sensor.Value;
                    t.Max = sensor.Max;

                    Temps.Add(t);
                }
            };

            foreach (IHardware hardware in computer.Hardware)
            {
                hardware.Update();

                switch (hardware.HardwareType)
                {
                    case HardwareType.Cpu:
                        foreach (ISensor sensor in hardware.Sensors)
                        {
                            if (sensor.SensorType != SensorType.Temperature)
                                continue;

                            if (!sensor.Name.StartsWith("Core Average"))
                                continue;

                            System.Console.WriteLine("CPU Temp: {0} {1}", sensor.Value, sensor.Max);

                            addToList(hardware, sensor, "CPU");
                        }
                        break;
                    case HardwareType.GpuNvidia:
                    case HardwareType.GpuAmd:
                    case HardwareType.GpuIntel:
                        foreach (ISensor sensor in hardware.Sensors)
                        {
                            if (sensor.SensorType != SensorType.Temperature)
                                continue;

                            addToList(hardware, sensor, "GPU");
                        }
                        break;
                    case HardwareType.Storage:
                        foreach (ISensor sensor in hardware.Sensors)
                        {
                            if (sensor.SensorType != SensorType.Temperature)
                                continue;

                            addToList(hardware, sensor, "SSD");
                        }
                        break;
                    default:
                        break;
                }
            }

            return Temps;
        }
    }

    public class HttpServer
    {
        public HttpListener? httpListenner;

        public void Start(HardwareMonitor Monitor, string url)
        {
            try
            {
                System.Console.WriteLine("HttpListener.Prefixes is: {0}", url);

                httpListenner = new HttpListener();
                httpListenner.AuthenticationSchemes = AuthenticationSchemes.Anonymous;
                httpListenner.Prefixes.Add(url);
                httpListenner.Start();

                System.Console.WriteLine("Start HttpListener successed");

                new Thread(new ThreadStart(delegate {
                    try
                    {
                        DoWork(Monitor, httpListenner);
                    }
                    catch (Exception)
                    {
                        httpListenner.Stop();
                    }
                })).Start();
            }
            catch (Exception e)
            {
                System.Console.WriteLine("Start HttpListener failed: {0}", e.Message);
            }
        }

        public void Stop()
        {
            httpListenner?.Stop();
        }

        private static void DoWork(HardwareMonitor Monitor, HttpListener httpListenner)
        {
            List<TemperatureInfo> Temps = new List<TemperatureInfo>();

            DateTime t1 = DateTime.Now;

            while (httpListenner.IsListening)
            {
                HttpListenerContext context = httpListenner.GetContext();
                HttpListenerRequest req = context.Request;
                HttpListenerResponse res = context.Response;

                res.SendChunked = false;

                if (req.HttpMethod == "GET")
                {
                    if(req.RawUrl== "/api/status/hardware/temperatures")
                    {
                        try
                        {
                            DateTime t2 = DateTime.Now;
                            TimeSpan ts = t2.Subtract(t1);
                            if (ts.TotalMilliseconds > 1000 || Temps.Count() == 0)
                            {
                                t1 = t2;
                                Temps = Monitor.GetTemperatures();
                            }

                            res.ContentType = "application/json";
                            res.StatusCode = 200;
                            string data = JsonConvert.SerializeObject(Temps);
                            byte[] bytes = Encoding.UTF8.GetBytes(data);
                            res.ContentLength64 = bytes.Length;
                            res.OutputStream.Write(bytes, 0, bytes.Length);
                        }
                        catch(Exception e)
                        {
                            System.Console.WriteLine("Handle http request failed: {0}", e.Message);
                        }
                    }
                    else
                    {
                        res.StatusCode = 400;
                        res.StatusDescription = "Bad Request";
                    }
                }
                else
                {
                    res.StatusCode = 400;
                    res.StatusDescription = "Bad Request";
                }

                res.Close();
            }
        }
    }

    public class Application
    {
        public static void Main(string[] args)
        {
            Console.Clear();

            string url = "http://127.0.0.1:28881/";

            if (args.Length > 0)
            {
                url = args[0];
            }

            HardwareMonitor Monitor = new HardwareMonitor();
            HttpServer httpServer = new HttpServer();

            Monitor.Open();
            httpServer.Start(Monitor, url);

            EventWaitHandle sig = new AutoResetEvent(false);

            // 注册 ctrl + c 信号，用来退出程序
            Console.CancelKeyPress += (sender, e) =>
            {
                // true: 不导致退出。false: 会导致退出
                e.Cancel = true;
                sig.Set();
            };

            sig.WaitOne();

            httpServer.Stop();
            Monitor.Close();

            System.Console.WriteLine("程序退出");
        }
    }
}
