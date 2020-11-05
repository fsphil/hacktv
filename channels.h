#include <pthread.h>
#include "video.h"

#define CHANNEL_OUTPUT_BUFFER_LINES 30

typedef struct _channel_t_struct {
	vid_t vid;

	/* Output buffer */
	int32_t *lines[CHANNEL_OUTPUT_BUFFER_LINES];
	int lines_filled;

	pthread_t process_thread;

	int thread_abort;

	/* Thread locking and signaling */
	pthread_mutex_t mutex;
	/* Signals when a new line is ready */
	pthread_cond_t ready_cond;
	/* Signals when space is available */
	pthread_cond_t free_cond;

	struct _channel_t_struct *next;

	/* Heterodyne state */
	int16_t offset_counter;
	cint32_t offset_phase;
	cint32_t offset_delta;
} _channel_t;

typedef struct {
	/* Signal configuration */
	vid_config_t conf;

	int width;
	unsigned int sample_rate;

	/* Linked list of channels */
	_channel_t *chans;

	/* Output buffers */
	int32_t *outline32;
	int16_t *outline;
} chans_t;

extern int chans_init(chans_t *c, unsigned int sample_rate, const vid_config_t * const conf);
extern void chans_free(chans_t *c);
extern int chans_test_add(chans_t *c, int offset_freq);
extern int chans_ffmpeg_add(chans_t *c, int offset_freq, char *input_url);
extern int16_t *chans_next_line(chans_t *c, size_t *samples);
