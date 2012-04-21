/*
 * ps3m2ts
 * 
 * Copyright (R) 2009-> Henning M. Stephansen
 * Feel free to use the code by any means, hopefully you can submit your improvements, ideas etc
 * to henningms@gmail.com or leave a comment at my blog http://www.henning.ms
 * 
 */

using System;
using System.Collections.Generic;
using System.Linq;
using System.IO;
using System.Diagnostics;
using System.Text;

namespace ps3m2ts
{
    class Support
    {
        public enum MediaType
        {
            Video = 0,
            Audio = 1,
            Subtitle = 2
        }
        
        public struct MediaInfo
        {
            // General mediainfo
            public int TrackID;
            public MediaType Type;
            public String Format;
            public String FormatInfo;
            public String CodecID;
            public String Duration;
            public int BitRate;
            public String Language;

            // Videospecific
            public int VideoWidth;
            public int VideoHeight;
            public String VideoFrameRate;
            public String VideoAspectRatio;
            public String Level;

            // External
            public string Filename;
        }

        public static List<MediaInfo> ReadMediaFile(string file)
        {
            try
            {
                // Assumes MediaInfo.exe exists
                if (File.Exists("mediainfo.exe"))
                {
                    System.Globalization.NumberFormatInfo ni = null;
                    System.Globalization.CultureInfo ci = System.Globalization.CultureInfo.InstalledUICulture;
                    ni = (System.Globalization.NumberFormatInfo)ci.NumberFormat.Clone();
                    ni.NumberDecimalSeparator = ".";


                    // Ok, so MediaInfo exists, lets start doing some work then
                    Process p = new Process();
                    p.StartInfo.FileName = "MediaInfo.exe";
                    p.StartInfo.Arguments = "-f \"" + file + "\"";
                    p.StartInfo.UseShellExecute = false;
                    p.StartInfo.RedirectStandardError = true;
                    p.StartInfo.RedirectStandardOutput = true;
                    p.StartInfo.CreateNoWindow = true;
                    p.StartInfo.WorkingDirectory = Environment.CurrentDirectory;
                    p.Start();

                    StringBuilder MediaInfoOutput = new StringBuilder();
                    while (!p.StandardOutput.EndOfStream)
                    {
                        MediaInfoOutput.AppendLine(p.StandardOutput.ReadLine());
                    }

                    String line = "";
                    String[] MediaInfoLines = MediaInfoOutput.ToString().Split(new String[] { Environment.NewLine }, StringSplitOptions.None);
                    List<MediaInfo> TrackList = new List<MediaInfo>();

                    for (int i= 0; i < MediaInfoLines.Length; i++)
                    {
                        line = MediaInfoLines[i];
                        if (line.StartsWith("General"))
                        {
                            while (MediaInfoLines[i] != "") i++;
                        }

                        else if (line.StartsWith("Video"))
                        {
                            MediaInfo tmpVideo = new MediaInfo();
                            tmpVideo.Type = MediaType.Video;
                            i++;

                            while (MediaInfoLines[i] != "")
                            {
                                line = MediaInfoLines[i];
                                int IndexOfDelimeter = line.IndexOf(':');

                                String info1 = line.Substring(0, IndexOfDelimeter).Trim();
                                String info2 = line.Substring(IndexOfDelimeter + 1, line.Length - IndexOfDelimeter - 1).Trim();

                                if (info1 == "Format")
                                    tmpVideo.Format = info2;
                                else if (info1.StartsWith("Format/Info"))
                                    tmpVideo.FormatInfo = info2;
                                else if (info1.StartsWith("Format profile"))
                                    tmpVideo.Level = info2;
                                else if (info1 == "Codec ID")
                                    tmpVideo.CodecID = info2;
                                else if (info1.StartsWith("Duration"))
                                    tmpVideo.Duration = info2;
                                else if (info1.StartsWith("Bit rate") && info2.ToLower().Contains("kbps"))
                                {
                                    String[] bitrate = info2.Split(new string[] { " " }, StringSplitOptions.None);
                                    tmpVideo.BitRate = Convert.ToInt16(bitrate[0], ni);
                                }
                                else if (info1.StartsWith("Width") && info2.ToLower().Contains("pixels"))
                                {
                                    String[] width = info2.Split(new string[] { " " }, StringSplitOptions.None);
                                    tmpVideo.VideoWidth = Convert.ToInt16(width[0]);
                                }
                                else if (info1.StartsWith("Height") && info2.ToLower().Contains("pixels"))
                                {
                                    String[] height = info2.Split(new string[] { " " }, StringSplitOptions.None);
                                    tmpVideo.VideoHeight = Convert.ToInt16(height[0]);
                                }
                                else if (info1.StartsWith("Frame rate") && info2.ToLower().Contains("fps"))
                                {
                                    String[] fps = info2.Split(new string[] { " " }, StringSplitOptions.None);

                                    tmpVideo.VideoFrameRate = fps[0];
                                }
                                else if (info1.StartsWith("Display aspect ratio"))
                                {
                                    tmpVideo.VideoAspectRatio = info2.Trim();
                                }
                                else if (info1.StartsWith("Language"))
                                {
                                    tmpVideo.Language = info2.Trim();
                                }
                                else if (info1 == "ID")
                                {
                                    tmpVideo.TrackID = Convert.ToInt16(info2.Trim(), ni);
                                }

                                i++;
                            }

                            TrackList.Add(tmpVideo);
                        }

                        else if (line.StartsWith("Audio"))
                        {
                            MediaInfo tmpAudio = new MediaInfo();
                            tmpAudio.Type = MediaType.Audio;
                            i++;

                            while (MediaInfoLines[i] != "")
                            {
                                line = MediaInfoLines[i];
                                int IndexOfDelimeter = line.IndexOf(':');

                                String info1 = line.Substring(0, IndexOfDelimeter).Trim();
                                String info2 = line.Substring(IndexOfDelimeter + 1, line.Length - IndexOfDelimeter - 1).Trim();

                                if (info1 == "Format")
                                    tmpAudio.Format = info2;
                                else if (info1.StartsWith("Format/Info"))
                                    tmpAudio.FormatInfo = info2;
                                else if (info1.StartsWith("Format profile"))
                                    tmpAudio.Level = info2;
                                else if (info1 == "Codec ID")
                                    tmpAudio.CodecID = info2;
                                else if (info1.StartsWith("Duration"))
                                    tmpAudio.Duration = info2;
                                else if (!info1.StartsWith("Bit rate mode") && info1.StartsWith("Bit rate") && info2.ToLower().Contains("kbps"))
                                {
                                    String[] bitrate = info2.Split(new string[] { " " }, StringSplitOptions.None);
                                    Decimal test = Decimal.Parse(bitrate[0], ni);
                                    tmpAudio.BitRate = Convert.ToInt16(test);
                                }
                                else if (info1.StartsWith("Language"))
                                {
                                    tmpAudio.Language = info2.Trim();
                                }
                                else if (info1 == "ID")
                                {
                                    tmpAudio.TrackID = Convert.ToInt16(info2.Trim(), ni);
                                }

                                i++;
                            }

                            TrackList.Add(tmpAudio);
                        }
                    }


                    return TrackList;
                }
                else
                {
                    return null;
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.Message + ex.StackTrace);
                Cleanup(file);
                Environment.Exit(1);
                return null;
            }
        }

        public static string WriteTSMuxerMetaFile(string file, List<MediaInfo> tracks, bool split, string outputformat)
        {
            string MetaFile = String.Empty;
            try
            {
                if (tracks != null && tracks.Count > 0)
                {
                    StreamWriter sw = new StreamWriter(Path.GetFileNameWithoutExtension(file) + ".meta");

                    MetaFile = "MUXOPT --no-pcr-on-video-pid --new-audio-pes --vbr --vbv-len=500";

                    if (split) MetaFile += " --split-size=4GB";
                    if ((outputformat == "blu-ray") || (outputformat == "avchd")) MetaFile += " --" + outputformat;
                       // sw.WriteLine("MUXOPT --no-pcr-on-video-pid --new-audio-pes --vbr --split-size=4GB --vbv-len=500");
                    //else
                      //  sw.WriteLine("MUXOPT --no-pcr-on-video-pid --new-audio-pes --vbr  --vbv-len=500");

                    MetaFile += Environment.NewLine;
                    //sw.WriteLine(muxopt);

                    file = file.Insert(0, "\"");
                    file = file.Insert(file.Length, "\"");

                    foreach (MediaInfo trackItem in tracks)
                    {
                        if (trackItem.Type == MediaType.Video)
                        {
                            string level = "";
                            if (trackItem.Level.Contains("5.1"))
                                level = "4.1";

                            if (level != "")
                                MetaFile += trackItem.CodecID + ", " + file + ", fps=" + trackItem.VideoFrameRate 
                                    + ", level=4.1, insertSEI, contSPS, ar=As source, track=" + trackItem.TrackID.ToString() + Environment.NewLine;
                            //sw.WriteLine(trackItem.CodecID + ", " + file + ", fps=" + trackItem.VideoFrameRate + ", level=4.1, insertSEI, contSPS, ar=As source, track=" + trackItem.TrackID.ToString());
                            else
                                MetaFile += trackItem.CodecID + ", " + file + ", fps=" + trackItem.VideoFrameRate 
                                    + ", insertSEI, contSPS, ar=As source, track=" + trackItem.TrackID.ToString() + Environment.NewLine;


                                //sw.WriteLine(trackItem.CodecID + ", " + file + ", fps=" + trackItem.VideoFrameRate + ", insertSEI, contSPS, ar=As source, track=" + trackItem.TrackID.ToString());
                        }
                        else if (trackItem.Type == MediaType.Audio)
                        {
                            if (trackItem.TrackID == 0)
                            {
                                // An external file
                                MetaFile += trackItem.CodecID + ", \"" + trackItem.Filename + "\"" + Environment.NewLine;
                                //sw.WriteLine(trackItem.CodecID + ", \"" + trackItem.Filename + "\"");
                            }
                            else
                            {
                                MetaFile += trackItem.CodecID + ", " + file + ", track=" + trackItem.TrackID.ToString() + Environment.NewLine;
                                //sw.WriteLine(trackItem.CodecID + ", " + file + ", track=" + trackItem.TrackID.ToString());
                            }
                        }
                    }

                    sw.Write(MetaFile);
                    sw.Close();

                }
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.Message);
                Cleanup(file);
                Environment.Exit(1);
            }

            return MetaFile;
        }

        public static void TSMuxerMuxFile(string file, string destination, string outputformat, Logger Log)
        {
            try
            {
                if (File.Exists(file))
                {
                    string metafile = Path.GetFileNameWithoutExtension(file) + ".meta";
                    string outputfile = destination + ((destination.EndsWith("\\")) ? "" : "\\");
                    if ((outputformat == "m2ts") || (outputformat == "ts"))
                        outputfile += Path.GetFileNameWithoutExtension(file) + ".";
                    outputfile += outputformat;

                    Log.Log("Starting tsMuxeR with output '" + outputfile + "'...");

                    if (File.Exists("tsmuxer.exe"))
                    {
                        Process p = new Process();
                        p.StartInfo.FileName = "tsmuxer.exe";
                        p.StartInfo.Arguments = "\"" + metafile + "\" \"" + outputfile + "\"";
                        p.StartInfo.UseShellExecute = false;
                        p.StartInfo.RedirectStandardError = true;
                        p.StartInfo.RedirectStandardOutput = true;
                        p.StartInfo.CreateNoWindow = true;
                        p.StartInfo.WorkingDirectory = Environment.CurrentDirectory;

                        
                        p.Start();

                        while (p.StandardOutput.EndOfStream != true)
                        {
                            Log.Log("tsMuxeR: " + p.StandardOutput.ReadLine());
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.Message);
                Cleanup(file);
                Environment.Exit(1);
            }
        }

        public static void ExtractMKV(string file, List<MediaInfo> tracks)
        {
            try
            {
                if (File.Exists(file))
                {
                    string arguments = "";
                    string fileWoEx = Path.GetFileNameWithoutExtension(file);

                    foreach (MediaInfo tmptrack in tracks)
                    {
                        if (tmptrack.CodecID == "V_MPEG4/ISO/AVC")
                            arguments += tmptrack.TrackID.ToString() + ":" + fileWoEx + ".h264 ";
                        else if (tmptrack.CodecID == "A_AC3")
                            arguments += tmptrack.TrackID.ToString() + ":" + fileWoEx + ".ac3 ";
                        else if (tmptrack.CodecID == "A_DTS")
                            arguments += tmptrack.TrackID.ToString() + ":" + fileWoEx + ".dts ";
                    }

                    if (File.Exists("mkvextract.exe"))
                    {
                        Process p = new Process();
                        p.StartInfo.FileName = "mkvextract.exe";
                        p.StartInfo.Arguments = "tracks \"" + file + "\" " + arguments; 
                        p.StartInfo.UseShellExecute = false;
                        p.StartInfo.RedirectStandardError = true;
                        p.StartInfo.RedirectStandardOutput = true;
                        p.StartInfo.CreateNoWindow = true;
                        p.StartInfo.WorkingDirectory = Environment.CurrentDirectory;
                        p.Start();

                        while (!p.StandardOutput.EndOfStream)
                        {
                            Console.WriteLine(p.StandardOutput.ReadLine());
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.Message);
                Cleanup(file);
                Environment.Exit(1);
            }
        }

        public static List<MediaInfo> ConvertDTS(string file, List<MediaInfo> tracks)
        {
            try
            {
                if (File.Exists(file))
                {
                    string fileWoEx = Path.GetFileNameWithoutExtension(file);
                    List<MediaInfo> tmpList = new List<MediaInfo>();

                    foreach (MediaInfo audioTracks in tracks)
                    {
                        if (audioTracks.Type == MediaType.Audio && audioTracks.CodecID == "A_DTS")
                        {
                            MediaInfo tmpAudioTrack = audioTracks;

                            if (File.Exists("eac3to\\eac3to.exe"))
                            {
                                Process p = new Process();
                                p.StartInfo.FileName = "eac3to\\eac3to.exe";
                                p.StartInfo.Arguments = "\"" + fileWoEx + ".dts\" \"" + fileWoEx + ".ac3\"";
                                p.StartInfo.UseShellExecute = false;
                                p.StartInfo.RedirectStandardError = true;
                                p.StartInfo.RedirectStandardOutput = true;
                                p.StartInfo.CreateNoWindow = true;
                                p.StartInfo.WorkingDirectory = Environment.CurrentDirectory;
                                p.Start();

                                while (!p.StandardOutput.EndOfStream)
                                {
                                    Console.WriteLine(p.StandardOutput.ReadLine());
                                }
                            }

                            if (File.Exists(fileWoEx + ".ac3"))
                            {

                                tmpAudioTrack.CodecID = "A_AC3";
                                tmpAudioTrack.TrackID = 0;
                                tmpAudioTrack.Filename = fileWoEx + ".ac3";
                            }

                            tmpList.Add(tmpAudioTrack);

                        }
                        else
                        {
                            tmpList.Add(audioTracks);
                        }
                    }

                    return tmpList;
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.Message);
                Cleanup(file);
                Environment.Exit(1);
            }

            return null;
        }

        public static void Cleanup(string file)
        {
            Support.Cleanup(file, false);
        }

        public static void Cleanup(string file, bool deletesource)
        {
            // Lets try to free some space

            string fileWoEx = Path.GetFileNameWithoutExtension(file);

            if (File.Exists(fileWoEx + ".h264"))
                File.Delete(fileWoEx + ".h264");

            if (File.Exists(fileWoEx + ".ac3"))
                File.Delete(fileWoEx + ".ac3");

            if (File.Exists(fileWoEx + ".dts"))
                File.Delete(fileWoEx + ".dts");

            if (File.Exists(fileWoEx + ".meta"))
                File.Delete(fileWoEx + ".meta");

            if (File.Exists(fileWoEx + " - Log.txt"))
                File.Delete(fileWoEx + " - Log.txt");

            if ((deletesource) && (File.Exists(file)))
                File.Delete(file);
        }

        public static Dictionary<string, string> ParseCommandLineArgs(string[] args)
        {
            if (args.Length < 1) return null;
            var options = new Dictionary<string, string>();

            options.Add("input", args[0]);

            for (var i = 1; i < args.Length; i++)
            {
                switch (args[i].ToLower())
                {
                    case "/split":
                        options.Add("split", "true");
                        break;

                    case "/log":
                        options.Add("log", "true");
                        break;

                    case "/format=ts":
                    case "/format=\"ts\"":
                        options.Add("outputformat", "ts");
                        break;

                    case "/format=blu-ray":
                    case "/format=\"blu-ray\"":
                        options.Add("outputformat", "blu-ray");
                        break;

                    case "/format=avchd":
                    case "/format=\"avchd\"":
                        options.Add("outputformat", "avchd");
                        break;

                    // default output format "m2ts" set later in this method.

                    case "/dest":
                    case "/destination":
                        i++;
                        if (i < args.Length) options.Add("destination", args[i]);
                        break;

                    case "/delsource":
                    case "/deletesource":
                        options.Add("deletesource", "true");
                        break;
                }
            }

            // set defaults
            if (!options.ContainsKey("outputformat")) options.Add("outputformat", "m2ts");

            return options;
        }

        public static void DisplayHelp()
        {
            Console.WriteLine("ps3m2ts usage: ps3m2ts \"<input-path>\" [/split] [/dest \"<output-path>\"]");
            Console.WriteLine("    [/format=<format>] [/delsource] [/log]");
            Console.WriteLine("");

            Console.WriteLine("  \"<input-path>\"\t The .mkv file or directory of files to convert.");
            Console.WriteLine("");

            Console.WriteLine("  /split\t\t Split the output to 4 GB files.");
            Console.WriteLine("  /dest \"<output-path>\"\t Path to save output (default is same as input).");
            Console.WriteLine("  /format=<format>\t Specify the output format:");
            Console.WriteLine("\t\t\t \"m2ts\" (default), \"ts\", \"blu-ray\", or \"avchd\".");
            Console.WriteLine("  /delsource\t\t Delete the input file(s) after conversion.");
            Console.WriteLine("  /log\t\t\t Enable conversion log (saves to input directory).");
            Console.WriteLine("");

            Console.WriteLine("Press any key to exit. . .");
            Console.ReadKey();
        }
    }
}
