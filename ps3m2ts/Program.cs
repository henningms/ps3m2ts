/*
 * ps3m2ts
 * 
 * Copyright (R) 2009-> Henning M. Stephansen
 * Feel free to use the code by any means, hopefully you can submit your improvements, ideas etc
 * to henningms@gmail.com or leave a comment at my blog http://www.henning.ms
 */

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;

namespace ps3m2ts
{
    class Program
    {
        static void Main(string[] args)
        {
            if ((args.Length == 0) || (args[0] == "/?"))
            {
                Support.DisplayHelp();
                Environment.Exit(1);
            }

            var options = Support.ParseCommandLineArgs(args);

            // set up the log
            var logDirectory = Environment.CurrentDirectory;

            if (options.ContainsKey("input"))
            {
                if (Directory.Exists(options["input"])) logDirectory = options["input"];
                else if (File.Exists(options["input"])) logDirectory = Path.GetDirectoryName(options["input"]);
            }

            var log = new Logger("ps3m2ts", (options.ContainsKey("log")), logDirectory);

            log.Log("ps3m2ts started...");

            if ((options == null) || (!options.ContainsKey("input")))
            {
                log.Log("Error: No input specified");
                Environment.Exit(1);
            }

            
            // check the destination is valid
            if (options.ContainsKey("destination"))
            {
                if (!Directory.Exists(options["destination"]))
                {
                    try
                    {
                        Directory.CreateDirectory(options["destination"]);
                    }
                    catch
                    {
                        log.Log("Error: Unable to create destination path '" + options["destination"] + "'.");
                        Environment.Exit(1);
                    }
                }
            }

            var inputFiles = new List<string>();

            // is it a single file?
            if (File.Exists(options["input"]))
            {
                inputFiles.Add(options["input"]);
            }

            // or is it a directory?
            else if (Directory.Exists(options["input"]))
            {
                var dir = new DirectoryInfo(options["input"]);
                var filesInDir = dir.GetFiles("*.mkv", SearchOption.AllDirectories);

                inputFiles.AddRange(filesInDir.Select(fileInDir => fileInDir.FullName));

                // were there any .mkv files in the directory?
                if (inputFiles.Count == 0)
                {
                    log.Log("Error: No .mkv files in directory '" + options["input"] + "'.");
                    Environment.Exit(1);
                }

                if ((inputFiles.Count > 1) &&
                    ((options["outputformat"] == "blu-ray") || (options["outputformat"] == "avchd")))
                {
                    // not compatible with blu-ray or avchd output formats
                    log.Log("Error: Can't convert multiple files to blu-ray or avchd format.");
                    Environment.Exit(1);
                }

            }

            // or doesn't it exist at all?
            else
            {
                log.Log("Error: Invalid input '" + options["input"] + "'.");
                Environment.Exit(1);
            }

            // process the input file(s)
            log.Log("Processing input '" + options["input"] + "'...");

            foreach (var inputFile in inputFiles)
            {
                log.Log("Processing file '" + inputFile + "'...");

                var destination = string.Empty;

                if (options.ContainsKey("destination")) destination = options["destination"];

                if (destination == string.Empty) destination = Path.GetDirectoryName(inputFile);
                if (destination == string.Empty) destination = ".";

                var fileTrackList = Support.ReadMediaFile(inputFile);

                if (fileTrackList != null && fileTrackList.Count > 0)
                {
                    var video = false;
                    var audio = false;
                    var dts = false;

                    var trackList = new List<Support.MediaInfo>();

                    foreach (var TrackInfo in fileTrackList)
                    {
                        if (TrackInfo.Type == Support.MediaType.Video && video == false)
                        {
                            trackList.Add(TrackInfo);
                            video = true;
                        }
                        else if (TrackInfo.Type == Support.MediaType.Audio && audio == false)
                        {
                            trackList.Add(TrackInfo);
                            audio = true;
                        }
                    }

                    if (trackList[1].Type == Support.MediaType.Audio && trackList[1].CodecID == "A_DTS")
                        dts = true;

                    if (dts)
                    {
                        Support.ExtractMKV(inputFile, trackList);
                        List<Support.MediaInfo> NewTrackList = Support.ConvertDTS(inputFile, trackList);
                        if (NewTrackList != null) trackList = NewTrackList;
                    }

                    String metafile = Support.WriteTSMuxerMetaFile(inputFile, trackList, (options.ContainsKey("split")), options["outputformat"]);
                    log.Log("Written .meta file:" + Environment.NewLine + metafile);

                    Support.TSMuxerMuxFile(inputFile, destination, options["outputformat"], log);
                    log.Log("Finished processing file '" + inputFile + "'.");
                    log.Log("-------------------------------------------------------------------");

                    // conversion must have been successful to be down here so clean up can delete the source (if requested).
                    Support.Cleanup(inputFile, (options.ContainsKey("deletesource")));

                }

                log.Log("ps3m2ts finished.");
            }
        }
    }
}
