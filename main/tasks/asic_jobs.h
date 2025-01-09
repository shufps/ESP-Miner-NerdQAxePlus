#pragma once

#include <pthread.h>
#include <string.h>

#include "mining.h"

#define MAX_ASIC_JOBS 128

class AsicJobs {
protected:
    bm_job *m_activeJobs[MAX_ASIC_JOBS];
    pthread_mutex_t m_validJobsLock;

    void lock() {
        pthread_mutex_lock(&m_validJobsLock);
    }

    void unlock() {
        pthread_mutex_unlock(&m_validJobsLock);
    }

    bm_job *cloneBmJob(bm_job *src)
    {
        bm_job *dst = (bm_job *) malloc(sizeof(bm_job));

        // copy all
        memcpy(dst, src, sizeof(bm_job));

        // copy strings
        dst->extranonce2 = strdup(src->extranonce2);
        dst->jobid = strdup(src->jobid);

        return dst;
    }

public:
    AsicJobs() {
        m_validJobsLock = PTHREAD_MUTEX_INITIALIZER;
        memset(m_activeJobs, 0, sizeof(m_activeJobs));
    }

    void cleanJobs() {
        lock();
        for (int i = 0; i < MAX_ASIC_JOBS; i++) {
            if (m_activeJobs[i]) {
                free_bm_job(m_activeJobs[i]);
                m_activeJobs[i] = 0;
            }
        }
        unlock();
    }

    void storeJob(bm_job *next_job, uint8_t asic_job_id) {
        lock();
        // if a slot was used before free it
        if (m_activeJobs[asic_job_id]) {
            free_bm_job(m_activeJobs[asic_job_id]);
        }
        // save job into slot
        m_activeJobs[asic_job_id] = next_job;
        unlock();
    }

    bm_job *getClone(uint8_t asic_job_id) {
        // check if we have a job with this job id
        lock();
        if (!m_activeJobs[asic_job_id]) {
            unlock();
            return NULL;
        }
        // create a clone
        bm_job *job = cloneBmJob(m_activeJobs[asic_job_id]);
        unlock();

        // and return it
        return job;
    }

};


