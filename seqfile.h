#ifndef _SEQFILE_H_
#define _SEQFILE_H_

#include <stdio.h>
#include <stdint.h>

#define ERROR_WHEN_NONENTITY 0
#define CREATE_WHEN_NONENTITY 1

#define OPERATE_OK 1
#define SEQFILE_INFOERR -1
#define SEQFILE_DIRERR -2
#define SEQFILE_NAMERR -3
#define SEQFILE_NONENTITYERR -4
#define SEQFILE_CREATERR -5
#define SEQFILE_CLOSERR -6
#define SEQFILE_CRTAGERR -7
#define SEQFILE_OPENERR -8
#define SEQFILE_TAGERR -9
#define SEQFILE_VERSIONERR -10
#define SEQFILE_SYNCTAGERR -11
#define SEQFILE_SYNCLENERR -12
#define SEQFILE_SYNCSTRERR -13
#define SEQFILE_GETPOSERR -14
#define SEQFILE_SEEKERR -15
#define SEQFILE_FORMERR -16
#define SEQFILE_TRUNCATERR -17
#define SEQFILE_FLUSHERR -18
#define SEQFILE_ENDOFILERR -19

#define SMLFILE_INFOERR -100
#define SMLFILE_NAMERR -101
#define SMLFILE_CONTENTERR -102
#define SMLFILE_SIZERR -103

struct seqfile_info {
    /* public */
    char pdir[128];
    char fname[128];
    uint8_t crt_tag;            /* ERROR_WHEN_NONENTITY, CREATE_WHEN_NONENTITY */
    
    /* private */
    FILE *fd;
    uint8_t end_tag;
    uint8_t sync_len;
    char sync_str[64];
    uint16_t head_len;
    uint64_t end_pos;
};

struct smlfile_info {
    char fname[256];
    uint32_t nlen;
    uint32_t clen;
    char fcontent[0x100000L];
};

extern int8_t open_seqfile(struct seqfile_info *seq_info);
extern int8_t write_seqfile(struct seqfile_info *seq_info, struct smlfile_info *sml_info);
extern int8_t flush_seqfile(struct seqfile_info *seq_info);
extern int8_t goto_1st_smlfile(struct seqfile_info *seq_info);
extern int8_t read_seqfile(struct seqfile_info *seq_info, struct smlfile_info *sml_info);
extern int8_t close_seqfile(struct seqfile_info *seq_info);

#endif
