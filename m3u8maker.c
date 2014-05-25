/*
 * Dialis AS Leif Einar Aune
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/timeb.h>
#ifndef INT64_C
#define INT64_C(c) (c ## LL)
#define UINT64_C(c) (c ## ULL)
#endif
#ifdef __cplusplus
extern "C" {
#endif
#include <libavformat/avformat.h>
#ifdef __cplusplus
}
#endif


static void initializeAvContext(AVFormatContext *input_context,
				int* video_index, int* audio_index) {
  int i=0;
  for (i = 0; i < input_context->nb_streams && (*video_index < 0 || *audio_index < 0); i++) {
    switch (input_context->streams[i]->codec->codec_type) {
    case CODEC_TYPE_VIDEO:
      *video_index = i;
      input_context->streams[i]->discard = AVDISCARD_NONE;
      break;
    case CODEC_TYPE_AUDIO:
      *audio_index = i;
      input_context->streams[i]->discard = AVDISCARD_NONE;
      break;
    default:
      fprintf(stderr, "SEGLOG: Dropping stream with type %d\n", input_context->streams[i]->codec->codec_type);
      input_context->streams[i]->discard = AVDISCARD_ALL;
      break;
    }
  }
}


int main(int argc, char **argv)
{
	int video_index = -1;
	int audio_index = -1;
	char* fileName;
	int ret = -1;
	AVInputFormat *input_format = NULL;
	AVFormatContext *input_context = NULL;
	int decode_done = 0;

	if (argc != 4) {
	  fprintf(stderr, "Usage: %s <input url> <chunkFrameCount> <framerate>\n", argv[0]);
	  return 1;
	}
	fileName = argv[1];
	int framesInChunk = atoi(argv[2]);
	int framerate = atoi(argv[3]);
	// ------------------ Done parsing input --------------

	av_register_all();

	input_format = av_find_input_format("mpegts");
	if (!input_format) {
	  fprintf(stderr, "SEGLOG: Segmenter error: Could not find MPEG-TS demuxer\n");
	  exit(1);
	}

	ret = av_open_input_file(&input_context, fileName, input_format, 0, NULL);
	if (ret != 0) {
	  fprintf(stderr, "SEGLOG: Segmenter error: Could not open input file, make sure it is an mpegts file: %d\n", ret);
	  exit(1);
	}

	input_context->max_analyze_duration = 60000000;
	if (av_find_stream_info(input_context) < 0) {
	  fprintf(stderr, "SEGLOG: Segmenter error: Could not read stream information\n");
	  exit(1);
	}

	initializeAvContext(input_context, &video_index, &audio_index);

	
	int frames = 0;
	int64_t firstChunkSize = 0;
	do {
	  AVPacket packet;
	  
	  decode_done = av_read_frame(input_context, &packet);
	  if (decode_done < 0) {
	    if (decode_done == AVERROR(EIO)) {
	      fprintf(stderr, "SEGLOG: IO error while reading the packet\n");
	      break;
	    } else
	      if (decode_done == AVERROR(EAGAIN)) {
		fprintf(stderr, "SEGLOG: trying again\n");
		continue;
	      }
	      else {
		fprintf(stderr, "SEGLOG: Unknown error: %d. Continuing.\n", decode_done);
		continue;
	      }
	  }
	  if (packet.stream_index == video_index) {
	    frames++;
	    // LEA: input_context->pb->pos is pointing at the position in the file for the current buffer
	    // pb->buf_ptr points where you afer, and buffer point to the buffer itself

	    /*
	    */

	    if ((packet.flags & PKT_FLAG_KEY) ) {
	      if (frames >= framesInChunk) {

		// Print first chunk as one byte less than we have read so far
		if (!firstChunkSize) {
		  printf("#EXTM3U\n");
		  printf("#EXT-X-PLAYLIST-TYPE:VOD\n");
		  printf("#EXT-X-TARGETDURATION:%d\n",frames/framerate);
		  printf("#EXT-X-VERSION:4\n");
		  printf("#EXT-X-MEDIA-SEQUENCE:0\n");

		  firstChunkSize = (input_context->pb->pos) - (int64_t)input_context->pb->buffer + (int64_t)input_context->pb->buf_ptr;
		  printf("#EXTINF:%d,\n",frames/framerate);
		  printf("#EXT-X-BYTERANGE:%d@0\n", firstChunkSize-1);
		  printf("%s\n",fileName);
		}
		//printf("We have one I frame, read %d since last I-frame\n",frames);
		printf("#EXTINF:%d,\n",frames/framerate);
		printf("#EXT-X-BYTERANGE:%d\n", (input_context->pb->pos) - (int64_t)input_context->pb->buffer + (int64_t)input_context->pb->buf_ptr);
		printf("%s\n",fileName);
		frames=0;
	      }
	    } 
	  }
	  
	  av_free_packet(&packet);
	} while (!decode_done);
	av_close_input_file(input_context);
	printf("#EXT-X-ENDLIST\n");
	return 0;
}
