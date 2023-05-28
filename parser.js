const fs = require("fs");
const readline = require("readline").createInterface({
  input: process.stdin,
  output: process.stdout,
});

class Video {
  constructor(title, timeStamp) {
    this.title = title;
    this.timeStamp = timeStamp;
  }
}

// 00:00:00 Name
let videoNameAfterRegex = /[a-z|A-z](\w|\d|\s|\W)+/gi;
// Name 00:00:00
let videoNameBeforeRegex = /[a-z|A-z](\w|\d|\s|\W)+(?=(\s\W?\d+:.+))/gi;
let timeStampRegex = /(\d+:)?\d+:\d\d/gi;

function getVideoName(line) {
  let match;
  if (line.match(videoNameBeforeRegex))
    match = line.match(videoNameBeforeRegex);
  else if (line.match(videoNameAfterRegex))
    match = line.match(videoNameAfterRegex);
  else match = [""];
  if(match[0].indexOf('\r') > 0)
  match[0] = match[0].slice(0,match[0].indexOf('\r'));
  return match[0].trim();
}

function getTimeStemp(line) {
  let match = line.match(timeStampRegex);
  if (match) return match[0].padStart(8, "00:00:00");
  return null;
}

let videos = [];

readline.question("", (path) => {
  fs.readFile(path, "utf8", (err, data) => {
    if (err) {
      console.error(err);
      return;
    }
    let lines = data.split("\n");
    lines.map((line) => {
      let timestamp = getTimeStemp(line);
      if (timestamp) {
        videos.push(new Video(getVideoName(line), timestamp));
      }
    });
    
    let numberOfvideos = new Buffer.allocUnsafe(4);
    numberOfvideos.writeInt32LE(videos.length);
    process.stdout.write(numberOfvideos);
    
    for (let i = 0; i < videos.length; i++) {
      process.stdout.write(Buffer.from(videos[i].timeStamp + '\0', 'ascii'));

      let titleLength = new Buffer.allocUnsafe(4);
      titleLength.writeInt32LE((i + 1).toString().length + 2 + videos[i].title.length + 5);
      process.stdout.write(titleLength);

      process.stdout.write(Buffer.from((i + 1).toString() + ") " + videos[i].title + ".mp4\0", 'ascii'));
    }
  });
  readline.close();
});
