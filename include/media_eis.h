#ifndef MEDIA_EIS_H
#define MEDIA_EIS_H

/* EIS 外部插件公开 API。字段沿用旧 libmedia 的源码级定义；当前插件仅实现 GLES NV12 路径。 */
#include <stdint.h>
#include "media_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int width;              /* 输入和输出图像宽度（像素）。 */
    int height;             /* 输入和输出图像高度（像素）。 */
    int format;             /* 像素格式；当前 GLES 路径要求 MEDIA_FORMAT_NV12。 */
    int input_depth;        /* 输入队列深度，0 使用插件默认值。 */
    int output_pool_id;     /* 输出 Buffer Pool ID，必须是已创建的 Pool。 */
    int input_stride;       /* 输入 linear plane0 行步长，0 使用布局默认值。 */
    int output_stride;      /* 输出 linear plane0 行步长，0 使用布局默认值。 */
    float crop_ratio;       /* 稳像保留比例；当前恒等 warp 不改变输出尺寸。 */
    int smoothing_window;   /* 运动平滑窗口，保留旧接口语义。 */
    int estimate_width;     /* 运动估计宽度，保留旧接口语义。 */
    int search_radius;      /* 特征搜索半径，保留旧接口语义。 */
    int block_step;         /* 特征块步长，保留旧接口语义。 */
    int passthrough;        /* 非零时启用直通复制路径。 */
} MEDIA_EIS_ATTR;

/*
 * EIS DIS-style semantic contract.
 *
 * These structures align field grouping, ranges and adjustment direction with
 * the public HiSilicon DIS CONFIG/ATTR/PARAM contract.  They intentionally do
 * not copy the HiSilicon ABI or proprietary algorithm.  IMAGE names the public
 * 4_DOF_GME semantic family without claiming this API change adds new motion
 * degrees of freedom to the existing image path; SENSOR maps to GYRO semantics,
 * and FUSION is an explicit rktohi extension.  The current production module
 * rejects a mode until its processing path is actually available; it never
 * silently falls back to IMAGE after accepting SENSOR/FUSION.
 */
typedef enum {
    MEDIA_EIS_MODE_IMAGE = 0,
    MEDIA_EIS_MODE_SENSOR = 1,
    MEDIA_EIS_MODE_FUSION = 2,
} MEDIA_EIS_MODE;

typedef enum {
    MEDIA_EIS_MOTION_LEVEL_LOW = 0,
    MEDIA_EIS_MOTION_LEVEL_NORMAL = 1,
    MEDIA_EIS_MOTION_LEVEL_HIGH = 2,
} MEDIA_EIS_MOTION_LEVEL;

typedef enum {
    MEDIA_EIS_PRODUCT_RECORDER = 0,
    MEDIA_EIS_PRODUCT_IPC = MEDIA_EIS_PRODUCT_RECORDER,
    MEDIA_EIS_PRODUCT_DV = 1,
    MEDIA_EIS_PRODUCT_DRONE = 2,
} MEDIA_EIS_PRODUCT_TYPE;

#define MEDIA_EIS_DIS_DEFAULT_BUFFER_NUM 5u
#define MEDIA_EIS_DIS_DEFAULT_CROP_RATIO 84u
#define MEDIA_EIS_DIS_DEFAULT_FRAME_RATE 60u
#define MEDIA_EIS_DIS_DEFAULT_HORIZONTAL_LIMIT 512u
#define MEDIA_EIS_DIS_DEFAULT_VERTICAL_LIMIT 512u
#define MEDIA_EIS_DIS_DEFAULT_STRENGTH 1024u
#define MEDIA_EIS_DIS_DEFAULT_LARGE_MOTION_STABLE_COEF 60u
#define MEDIA_EIS_DIS_DEFAULT_LOW_FREQ_MOTION_PRESERVE 50u
#define MEDIA_EIS_DIS_DEFAULT_LOW_FREQ_MOTION_FREQ 30u

typedef struct {
    MEDIA_EIS_MODE mode; /* 兼容旧 EIS API 字段。 */
    MEDIA_EIS_MOTION_LEVEL motion_level; /* 兼容旧 EIS API 字段。 */
    MEDIA_EIS_PRODUCT_TYPE product_type; /* 兼容旧 EIS API 字段。 */
    uint32_t buffer_num;             /* 5..10; EIS DMA-BUF import-cache budget, not look-ahead */ /* 中文字段说明见旧 API 语义。 */
    uint32_t crop_ratio;             /* retained output percent: 50..98; 100=rktohi zero-crop extension */ /* 中文字段说明见旧 API 语义。 */
    uint32_t frame_rate;             /* canonical range: 1..120; must match actual input */ /* 中文字段说明见旧 API 语义。 */
    uint32_t gyro_output_range_dps;  /* raw gyro declaration; calibrated float rad/s input keeps 0 */ /* 中文字段说明见旧 API 语义。 */
    uint32_t gyro_data_bit_width;    /* raw gyro declaration; calibrated float rad/s input keeps 0 */ /* 中文字段说明见旧 API 语义。 */
    int camera_steady; /* 兼容旧 EIS API 字段。 */
    int scale_output;                /* fixed-size output currently requires 1 */ /* 中文字段说明见旧 API 语义。 */
} MEDIA_EIS_DIS_CONFIG;

typedef struct {
    int enable;                      /* processing enable; Start/Stop remain lifecycle APIs */ /* 中文字段说明见旧 API 语义。 */
    int gdc_bypass; /* 兼容旧 EIS API 字段。 */
    uint32_t moving_subject_level;   /* 0..6 */ /* 中文字段说明见旧 API 语义。 */
    int32_t rolling_shutter_coef;    /* 0..1000; non-zero requires a supported row-warp path */
    int32_t timelag_us;              /* -2000000..2000000; corrected_ts=sample_ts-timelag_us */
    uint32_t horizontal_limit;       /* 0..1000 */ /* 中文字段说明见旧 API 语义。 */
    uint32_t vertical_limit;         /* 0..1000 */ /* 中文字段说明见旧 API 语义。 */
    int still_crop;                  /* keep configured crop while enable=0 */ /* 中文字段说明见旧 API 语义。 */
    uint32_t strength;               /* 0..1024 */ /* 中文字段说明见旧 API 语义。 */
} MEDIA_EIS_DIS_ATTR;

typedef struct {
    uint32_t large_motion_stable_coef; /* 0..100; larger gives large motion more correction authority */ /* 中文字段说明见旧 API 语义。 */
    uint32_t low_freq_motion_preserve; /* 0..100; larger preserves more intentional low-frequency motion */ /* 中文字段说明见旧 API 语义。 */
    uint32_t low_freq_motion_freq;     /* 0..100; smaller means lower cutoff and stronger stabilization */ /* 中文字段说明见旧 API 语义。 */
} MEDIA_EIS_DIS_PARAM;

/*
 * EIS six-axis IMU input contract.
 *
 * timestamp_us uses the same monotonic microsecond clock as
 * MEDIA_BUFFER_META.time_stamp.  Gyro values are right-hand-rule rad/s and
 * accelerometer values are m/s^2 in IMU axes.  rotation_cam_from_imu converts
 * an IMU-axis vector to OpenCV camera axes (x right, y down, z forward).
 * timelag_us remains the single time-offset source in MEDIA_EIS_DIS_ATTR:
 * corrected_timestamp_us = sample.timestamp_us - timelag_us.
 */
#define MEDIA_EIS_IMU_VALID_GYRO (1u << 0)
#define MEDIA_EIS_IMU_VALID_ACCEL (1u << 1)
#define MEDIA_EIS_IMU_VALID_MASK \
    (MEDIA_EIS_IMU_VALID_GYRO | MEDIA_EIS_IMU_VALID_ACCEL)

#define MEDIA_EIS_IMU_DEFAULT_QUEUE_CAPACITY 512u
#define MEDIA_EIS_IMU_DEFAULT_MAX_HISTORY_US 2000000u
#define MEDIA_EIS_IMU_DEFAULT_MIN_SAMPLE_RATE_HZ 100u
#define MEDIA_EIS_IMU_DEFAULT_MAX_SAMPLE_GAP_US 20000u
#define MEDIA_EIS_IMU_DEFAULT_MAX_SYNC_ERROR_US 2000u
#define MEDIA_EIS_IMU_DEFAULT_GYRO_SATURATION_RAD_S 20.0f

typedef enum {
    MEDIA_EIS_IMU_SCAN_UNKNOWN = 0,
    MEDIA_EIS_IMU_SCAN_TOP_TO_BOTTOM = 1,
    MEDIA_EIS_IMU_SCAN_BOTTOM_TO_TOP = 2,
    MEDIA_EIS_IMU_SCAN_LEFT_TO_RIGHT = 3,
    MEDIA_EIS_IMU_SCAN_RIGHT_TO_LEFT = 4,
} MEDIA_EIS_IMU_SCAN_DIRECTION;

typedef enum {
    MEDIA_EIS_IMU_INTERVAL_NONE = 0,
    MEDIA_EIS_IMU_INTERVAL_WAITING_FIRST_FRAME = 1,
    MEDIA_EIS_IMU_INTERVAL_OK = 2,
    MEDIA_EIS_IMU_INTERVAL_FRAME_TIMESTAMP_MISSING = 3,
    MEDIA_EIS_IMU_INTERVAL_FRAME_TIMESTAMP_NON_MONOTONIC = 4,
    MEDIA_EIS_IMU_INTERVAL_CALIBRATION_MISSING = 5,
    MEDIA_EIS_IMU_INTERVAL_SYNC_ERROR = 6,
    MEDIA_EIS_IMU_INTERVAL_OUTSIDE_COVERAGE = 7,
    MEDIA_EIS_IMU_INTERVAL_SAMPLE_GAP = 8,
    MEDIA_EIS_IMU_INTERVAL_SAMPLE_RATE_LOW = 9,
    MEDIA_EIS_IMU_INTERVAL_GYRO_SATURATED = 10,
} MEDIA_EIS_IMU_INTERVAL_STATUS;

typedef struct {
    uint32_t queue_capacity;       /* 8..4096 samples; bounded ring capacity */ /* 中文字段说明见旧 API 语义。 */
    uint32_t max_history_us;       /* 50000..60000000; older samples are dropped */ /* 中文字段说明见旧 API 语义。 */
    uint32_t min_sample_rate_hz;   /* 1..4000; default production floor is 100Hz */ /* 中文字段说明见旧 API 语义。 */
    uint32_t max_sample_gap_us;    /* 100..1000000; default 20ms */ /* 中文字段说明见旧 API 语义。 */
    uint32_t max_sync_error_us;    /* 0..1000000; default calibrated residual 2ms */ /* 中文字段说明见旧 API 语义。 */
    float gyro_saturation_rad_s;   /* finite 0.1..100; default 20rad/s */ /* 中文字段说明见旧 API 语义。 */
} MEDIA_EIS_IMU_CONFIG;
/* queue_capacity must cover max_history_us at min_sample_rate_hz plus two
 * interpolation boundary samples; max_history_us must cover at least two
 * max_sample_gap_us intervals. */

typedef struct {
    uint32_t image_width;  /* must match the EIS group */ /* 中文字段说明见旧 API 语义。 */
    uint32_t image_height; /* must match the EIS group */ /* 中文字段说明见旧 API 语义。 */
    float fx; /* 兼容旧 EIS API 字段。 */
    float fy; /* 兼容旧 EIS API 字段。 */
    float cx; /* 兼容旧 EIS API 字段。 */
    float cy; /* 兼容旧 EIS API 字段。 */
    float distortion[4];           /* k1, k2, p1, p2; zero when unavailable */ /* 中文字段说明见旧 API 语义。 */
    float rotation_cam_from_imu[9]; /* row-major 3x3 rotation */ /* 中文字段说明见旧 API 语义。 */
    float gyro_bias_rad_s[3]; /* 兼容旧 EIS API 字段。 */
    float accel_bias_m_s2[3]; /* 兼容旧 EIS API 字段。 */
    uint32_t timestamp_sync_error_us; /* 0..1000000; above config max is not usable */ /* 中文字段说明见旧 API 语义。 */
    uint32_t readout_time_us;          /* 0..1000000; nonzero requires scan direction */ /* 中文字段说明见旧 API 语义。 */
    MEDIA_EIS_IMU_SCAN_DIRECTION scan_direction; /* 兼容旧 EIS API 字段。 */
} MEDIA_EIS_IMU_CALIBRATION;

typedef struct {
    uint64_t timestamp_us; /* 兼容旧 EIS API 字段。 */
    float gyro_rad_s[3]; /* 兼容旧 EIS API 字段。 */
    float accel_m_s2[3]; /* 兼容旧 EIS API 字段。 */
    uint32_t valid_mask; /* gyro is required; accel may be marked invalid */ /* 中文字段说明见旧 API 语义。 */
} MEDIA_EIS_IMU_SAMPLE;

typedef struct {
    uint64_t epoch; /* 兼容旧 EIS API 字段。 */
    int calibration_valid; /* structurally valid; see interval status for sync */ /* 中文字段说明见旧 API 语义。 */
    uint32_t queue_count; /* 兼容旧 EIS API 字段。 */
    uint32_t queue_capacity; /* 兼容旧 EIS API 字段。 */
    uint64_t oldest_timestamp_us; /* 兼容旧 EIS API 字段。 */
    uint64_t newest_timestamp_us; /* 兼容旧 EIS API 字段。 */
    uint64_t last_accepted_timestamp_us; /* 兼容旧 EIS API 字段。 */
    float observed_sample_rate_hz; /* 兼容旧 EIS API 字段。 */
    uint32_t max_observed_gap_us; /* 兼容旧 EIS API 字段。 */
    uint64_t submitted_samples; /* 兼容旧 EIS API 字段。 */
    uint64_t accepted_samples; /* 兼容旧 EIS API 字段。 */
    uint64_t rejected_samples; /* 兼容旧 EIS API 字段。 */
    uint64_t rejected_non_monotonic_samples; /* 兼容旧 EIS API 字段。 */
    uint64_t rejected_non_finite_samples; /* 兼容旧 EIS API 字段。 */
    uint64_t rejected_invalid_mask_samples; /* 兼容旧 EIS API 字段。 */
    uint64_t saturated_gyro_samples; /* 兼容旧 EIS API 字段。 */
    uint64_t dropped_capacity_samples; /* 兼容旧 EIS API 字段。 */
    uint64_t dropped_history_samples; /* 兼容旧 EIS API 字段。 */
    uint64_t last_frame_timestamp_us; /* 兼容旧 EIS API 字段。 */
    uint64_t last_interval_start_us; /* 兼容旧 EIS API 字段。 */
    uint64_t last_interval_end_us; /* 兼容旧 EIS API 字段。 */
    MEDIA_EIS_IMU_INTERVAL_STATUS last_interval_status; /* 兼容旧 EIS API 字段。 */
    uint32_t last_interval_sample_count; /* 兼容旧 EIS API 字段。 */
    uint32_t last_interval_max_gap_us; /* 兼容旧 EIS API 字段。 */
    uint32_t calibration_sync_error_us; /* 兼容旧 EIS API 字段。 */
    float last_delta_angle_rad[3]; /* 兼容旧 EIS API 字段。 */
    float last_accel_mean_m_s2[3]; /* 兼容旧 EIS API 字段。 */
    int last_accel_valid; /* 兼容旧 EIS API 字段。 */
} MEDIA_EIS_IMU_STATS;

typedef struct {
    int frame_index; /* 兼容旧 EIS API 字段。 */
    int fallback_used; /* 兼容旧 EIS API 字段。 */
    int fallback_reason; /* 兼容旧 EIS API 字段。 */
    float raw_dx; /* 兼容旧 EIS API 字段。 */
    float raw_dy; /* 兼容旧 EIS API 字段。 */
    float raw_angle; /* 兼容旧 EIS API 字段。 */
    float smooth_dx; /* 兼容旧 EIS API 字段。 */
    float smooth_dy; /* 兼容旧 EIS API 字段。 */
    float smooth_angle; /* 兼容旧 EIS API 字段。 */
    float comp_dx; /* 兼容旧 EIS API 字段。 */
    float comp_dy; /* 兼容旧 EIS API 字段。 */
    float comp_angle; /* 兼容旧 EIS API 字段。 */
    float matrix[6]; /* 兼容旧 EIS API 字段。 */
    double estimate_ms; /* 兼容旧 EIS API 字段。 */
    double warp_ms; /* 兼容旧 EIS API 字段。 */
    double total_ms; /* 兼容旧 EIS API 字段。 */
    int warp_path; /* 0=cpu, 1=rga, 2=opencl, 3=opengles */ /* 中文字段说明见旧 API 语义。 */
    int estimate_path; /* 0=cpu image, 1=opencl image, 2=sensor, 3=fusion */ /* 中文字段说明见旧 API 语义。 */
    double estimate_kernel_ms; /* 兼容旧 EIS API 字段。 */
} MEDIA_EIS_STATS;

/*
 * SENSOR-specific diagnostics are kept separate from MEDIA_EIS_STATS so the
 * legacy six-float affine layout remains ABI compatible.  matrix is the exact
 * row-major source-to-output 3x3 homography used for the frame, including the
 * configured center crop/scale; inverse_matrix is the output-to-source matrix
 * uploaded to the selected GPU warp backend.
 */
typedef enum {
    MEDIA_EIS_SENSOR_SOURCE_NONE = 0,            /* no valid interval has established a correction */
    MEDIA_EIS_SENSOR_SOURCE_IMU = 1,
    MEDIA_EIS_SENSOR_SOURCE_LAST_STABLE = 2,     /* bounded hold after at least one valid interval */
    MEDIA_EIS_SENSOR_SOURCE_IDENTITY_RETURN = 3, /* continuous decay after the bounded hold */
    MEDIA_EIS_SENSOR_SOURCE_DISABLED = 4,
} MEDIA_EIS_SENSOR_SOURCE;

typedef struct {
    int frame_index; /* 兼容旧 EIS API 字段。 */
    MEDIA_EIS_MODE mode; /* 兼容旧 EIS API 字段。 */
    MEDIA_EIS_IMU_INTERVAL_STATUS interval_status; /* 兼容旧 EIS API 字段。 */
    MEDIA_EIS_SENSOR_SOURCE source; /* 兼容旧 EIS API 字段。 */
    uint32_t invalid_streak; /* 兼容旧 EIS API 字段。 */
    int visual_estimate_used; /* 兼容旧 EIS API 字段。 */
    float gyro_delta_angle_rad[3]; /* physical camera/body rotation over the frame interval */ /* 中文字段说明见旧 API 语义。 */
    float correction_angle_rad[3]; /* source-to-output compensation before crop allocation */ /* 中文字段说明见旧 API 语义。 */
    float applied_angle_rad[3];    /* crop-limited source-to-output compensation */ /* 中文字段说明见旧 API 语义。 */
    float matrix[9]; /* 兼容旧 EIS API 字段。 */
    float inverse_matrix[9]; /* 兼容旧 EIS API 字段。 */
    float roll_scale; /* 兼容旧 EIS API 字段。 */
    float tilt_scale; /* 兼容旧 EIS API 字段。 */
    float crop_scale; /* 兼容旧 EIS API 字段。 */
    float geometric_fill_ratio; /* 兼容旧 EIS API 字段。 */
    float accel_mean_m_s2[3]; /* 兼容旧 EIS API 字段。 */
    int accel_valid; /* 兼容旧 EIS API 字段。 */
    double controller_ms; /* 兼容旧 EIS API 字段。 */
    uint64_t sensor_frames; /* 兼容旧 EIS API 字段。 */
    uint64_t sensor_visual_estimate_calls; /* 兼容旧 EIS API 字段。 */
} MEDIA_EIS_SENSOR_STATS;

/* FUSION diagnostics use a separate structure so IMAGE and SENSOR ABI layouts
 * remain unchanged.  The source enum records the fixed continuity order used
 * by the production controller; depth_rank is a causal monocular LK signal,
 * not an external depth input or public tuning surface.
 */
typedef enum {
    MEDIA_EIS_FUSION_SOURCE_DISABLED = 0,
    MEDIA_EIS_FUSION_SOURCE_FUSED = 1,
    MEDIA_EIS_FUSION_SOURCE_IMU_ONLY = 2,
    MEDIA_EIS_FUSION_SOURCE_IMAGE_ONLY = 3,
    MEDIA_EIS_FUSION_SOURCE_LAST_STABLE = 4,
    MEDIA_EIS_FUSION_SOURCE_IDENTITY_RETURN = 5,
} MEDIA_EIS_FUSION_SOURCE;

typedef enum {
    MEDIA_EIS_FUSION_FALLBACK_NONE = 0,
    MEDIA_EIS_FUSION_FALLBACK_WAITING = 1,
    MEDIA_EIS_FUSION_FALLBACK_VISUAL_RESIDUAL_INVALID = 2,
    MEDIA_EIS_FUSION_FALLBACK_IMU_INVALID = 3,
    MEDIA_EIS_FUSION_FALLBACK_BOTH_INVALID_HOLD = 4,
    MEDIA_EIS_FUSION_FALLBACK_BOTH_INVALID_RETURN = 5,
    MEDIA_EIS_FUSION_FALLBACK_SCENE_CUT = 6,
    MEDIA_EIS_FUSION_FALLBACK_SOURCE_CONFLICT = 7,
} MEDIA_EIS_FUSION_FALLBACK_REASON;

typedef struct {
    int frame_index; /* 兼容旧 EIS API 字段。 */
    MEDIA_EIS_MODE mode; /* 兼容旧 EIS API 字段。 */
    MEDIA_EIS_IMU_INTERVAL_STATUS interval_status; /* 兼容旧 EIS API 字段。 */
    MEDIA_EIS_FUSION_SOURCE source; /* 兼容旧 EIS API 字段。 */
    MEDIA_EIS_FUSION_FALLBACK_REASON fallback_reason; /* 兼容旧 EIS API 字段。 */
    uint32_t invalid_streak; /* 兼容旧 EIS API 字段。 */
    int visual_fallback_reason; /* 兼容旧 EIS API 字段。 */
    int visual_tracks; /* 兼容旧 EIS API 字段。 */
    int visual_inliers; /* 兼容旧 EIS API 字段。 */
    int residual_inliers; /* 兼容旧 EIS API 字段。 */
    int depth_valid; /* 兼容旧 EIS API 字段。 */
    int persistent_eligible; /* 兼容旧 EIS API 字段。 */
    int persistent_used; /* 兼容旧 EIS API 字段。 */
    int current_far_used; /* 兼容旧 EIS API 字段。 */
    float visual_confidence; /* 兼容旧 EIS API 字段。 */
    float sensor_confidence; /* 兼容旧 EIS API 字段。 */
    float fusion_confidence; /* 兼容旧 EIS API 字段。 */
    float visual_increment[3]; /* dx px, dy px, angle rad */ /* 中文字段说明见旧 API 语义。 */
    float gyro_increment[3];   /* center-local dx/dy and angle */ /* 中文字段说明见旧 API 语义。 */
    float fused_increment[3]; /* 兼容旧 EIS API 字段。 */
    float residual_increment[3]; /* 兼容旧 EIS API 字段。 */
    float correction[3];       /* desired source-to-output dx/dy/angle */ /* 中文字段说明见旧 API 语义。 */
    float applied_correction[3]; /* 兼容旧 EIS API 字段。 */
    float matrix[9]; /* 兼容旧 EIS API 字段。 */
    float inverse_matrix[9]; /* 兼容旧 EIS API 字段。 */
    float rotation_scale; /* 兼容旧 EIS API 字段。 */
    float translation_scale; /* 兼容旧 EIS API 字段。 */
    float crop_scale; /* 兼容旧 EIS API 字段。 */
    float geometric_fill_ratio; /* 兼容旧 EIS API 字段。 */
    float direction_condition; /* 兼容旧 EIS API 字段。 */
    float residual_error_p95_px; /* 兼容旧 EIS API 字段。 */
    double geometry_ms; /* 兼容旧 EIS API 字段。 */
    double controller_ms; /* 兼容旧 EIS API 字段。 */
    uint64_t fusion_frames; /* 兼容旧 EIS API 字段。 */
    uint64_t fused_frames; /* 兼容旧 EIS API 字段。 */
    uint64_t imu_only_frames; /* 兼容旧 EIS API 字段。 */
    uint64_t image_only_frames; /* 兼容旧 EIS API 字段。 */
    uint64_t last_stable_frames; /* 兼容旧 EIS API 字段。 */
    uint64_t identity_return_frames; /* 兼容旧 EIS API 字段。 */
} MEDIA_EIS_FUSION_STATS;

int MEDIA_EIS_CreateGrp(int grp, const MEDIA_EIS_ATTR *attr); /* 兼容旧 EIS API 字段。 */
int MEDIA_EIS_DestroyGrp(int grp); /* 兼容旧 EIS API 字段。 */
int MEDIA_EIS_Start(int grp); /* 兼容旧 EIS API 字段。 */
int MEDIA_EIS_Stop(int grp); /* 兼容旧 EIS API 字段。 */
int MEDIA_EIS_Enable(int grp); /* 兼容旧 EIS API 字段。 */
int MEDIA_EIS_Disable(int grp); /* 兼容旧 EIS API 字段。 */
int MEDIA_EIS_SendFrame(int grp, MEDIA_BUFFER buf, int timeout_ms); /* 兼容旧 EIS API 字段。 */
int MEDIA_EIS_GetFrame(int grp, MEDIA_BUFFER *buf, int timeout_ms); /* 兼容旧 EIS API 字段。 */
int MEDIA_EIS_ReleaseFrame(int grp, MEDIA_BUFFER buf); /* 兼容旧 EIS API 字段。 */
int MEDIA_EIS_SetPassthrough(int grp, int enable); /* 兼容旧 EIS API 字段。 */
int MEDIA_EIS_GetStats(int grp, MEDIA_EIS_STATS *stats); /* 兼容旧 EIS API 字段。 */
/* CONFIG and PARAM updates require the group to be stopped.  While running,
 * ATTR currently permits only enable/still_crop changes; other changes return
 * MEDIA_ERR_PIPELINE_STATE to avoid a correction jump. */
int MEDIA_EIS_SetDISConfig(int grp, const MEDIA_EIS_DIS_CONFIG *config); /* 兼容旧 EIS API 字段。 */
int MEDIA_EIS_GetDISConfig(int grp, MEDIA_EIS_DIS_CONFIG *config); /* 兼容旧 EIS API 字段。 */
int MEDIA_EIS_SetDISAttr(int grp, const MEDIA_EIS_DIS_ATTR *attr); /* 兼容旧 EIS API 字段。 */
int MEDIA_EIS_GetDISAttr(int grp, MEDIA_EIS_DIS_ATTR *attr); /* 兼容旧 EIS API 字段。 */
int MEDIA_EIS_SetDISParam(int grp, const MEDIA_EIS_DIS_PARAM *param); /* 兼容旧 EIS API 字段。 */
int MEDIA_EIS_GetDISParam(int grp, MEDIA_EIS_DIS_PARAM *param); /* 兼容旧 EIS API 字段。 */
/* IMU CONFIG/CALIBRATION updates require stopped state.  Samples may be sent
 * singly or in an all-or-nothing strictly increasing batch while stopped or
 * running.  Send copies samples before returning; caller retains ownership.
 * Config, Stop and calibration/timelag changes clear the queued epoch. */
int MEDIA_EIS_SetIMUConfig(int grp, const MEDIA_EIS_IMU_CONFIG *config); /* 兼容旧 EIS API 字段。 */
int MEDIA_EIS_GetIMUConfig(int grp, MEDIA_EIS_IMU_CONFIG *config); /* 兼容旧 EIS API 字段。 */
int MEDIA_EIS_SetIMUCalibration(int grp,
                                const MEDIA_EIS_IMU_CALIBRATION *calibration);
int MEDIA_EIS_GetIMUCalibration(int grp,
                                MEDIA_EIS_IMU_CALIBRATION *calibration); /* 兼容旧 EIS API 字段。 */
int MEDIA_EIS_SendIMUSample(int grp, const MEDIA_EIS_IMU_SAMPLE *sample); /* 兼容旧 EIS API 字段。 */
int MEDIA_EIS_SendIMUSamples(int grp,
                             const MEDIA_EIS_IMU_SAMPLE *samples,
                             uint32_t count); /* 兼容旧 EIS API 字段。 */
int MEDIA_EIS_GetIMUStats(int grp, MEDIA_EIS_IMU_STATS *stats); /* 兼容旧 EIS API 字段。 */
int MEDIA_EIS_GetSensorStats(int grp, MEDIA_EIS_SENSOR_STATS *stats); /* 兼容旧 EIS API 字段。 */
int MEDIA_EIS_GetFusionStats(int grp, MEDIA_EIS_FUSION_STATS *stats); /* 兼容旧 EIS API 字段。 */


#ifdef __cplusplus
}
#endif

#endif /* MEDIA_EIS_H */
