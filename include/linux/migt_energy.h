#ifndef MIGT_ENERGY_H
#define MIGT_ENERGY_H

#ifdef CONFIG_MIGT_ENERGY_MODEL
int gpu_ea_start(struct devfreq *devf);
int gpu_ea_stop(struct devfreq *devf);
int gpu_ea_update_stats(struct devfreq *devf, u64 busy, u64 wall);
#else
static inline int gpu_ea_start(struct devfreq *devf) { return 0;}
static inline int gpu_ea_stop(struct devfreq *devf) { return 0;}
static inline int gpu_ea_update_stats(struct devfreq *devf,
		u64 busy, u64 wall) { return 0;}
#endif

#endif
