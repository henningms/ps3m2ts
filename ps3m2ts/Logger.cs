using System;
using System.IO;

namespace ps3m2ts
{
    public class Logger
    {
        #region Constructor

        public Logger(String AppName, Boolean LogToFile) : this(AppName, LogToFile, Environment.CurrentDirectory)
        {
        }

        public Logger(String AppName, Boolean LogToFile, String LogDirectory)
        {
            if (LogToFile)
            {
                this.LogToFile = LogToFile;

                String LogFileName = String.Format(@"{0}\{1} {2:yyyy-MM-dd HH-mm-ss}.log", LogDirectory, AppName,
                                                   DateTime.Now);
                LogWriter = new StreamWriter(LogFileName);
                LogWriter.AutoFlush = true;
            }
        }

        #endregion

        #region Private Fields

        private readonly Boolean LogToFile;
        private readonly StreamWriter LogWriter;

        #endregion

        #region Public Methods

        public void Log(String LogText)
        {
            if ((LogToFile) && (LogWriter != null))
            {
                LogText = String.Format("[{0:HH:mm:ss}] {1}", DateTime.Now, LogText);
                try
                {
                    LogWriter.WriteLine(LogText);
                }
                catch
                {
                }
            }

            Console.WriteLine(LogText);
        }

        public void Close()
        {
            try
            {
                LogWriter.Flush();
                LogWriter.Close();
            }
            catch
            {
            }
        }

        #endregion

        #region Private Methods

        ~Logger()
        {
            Close();
        }

        #endregion
    }
}