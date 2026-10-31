#!/usr/bin/env python3
"""Compile the unchanged production telemetry function with host platform seams.

Usage: python3 test/influx_best_difficulty_test.py [source-root]
The optional root permits the exact same test against the unpatched base.
"""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path(__file__).resolve().parents[1]


def function(text, signature):
    start = text.index(signature)
    brace = text.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


fetch = function((ROOT / 'main/tasks/influx_task.cpp').read_text(),
                 'static void influx_task_fetch_from_stratum_manager(')
source = r'''
#include <cassert>
#include <cstdint>
#include "influx.h"
// Only the Stratum/platform boundary is replaced, not the telemetry algorithm.
struct StratumManager {
    uint64_t session = 0, persisted = 0;
    uint64_t getBestSessionDiff() { return session; }
    uint64_t getBestDiff() { return persisted; }
    int getSharesAccepted() { return 11; }
    int getSharesRejected() { return 2; }
    int getPoolErrors() { return 3; }
    int getPoolDifficulty() { return 4096; }
    int getFoundBlocks() { return 0; }
};
uint64_t getDuplicateHWNonces() { return 4; }
static Influx *influxdb;
int last_block_found = 0;
Influx::Influx() {}
'''+fetch+r'''
void check(StratumManager &manager, uint64_t session, uint64_t persisted, float remote) {
    manager.session = session;
    manager.persisted = persisted;
    influxdb->m_stats.total_best_difficulty = remote;
    influx_task_fetch_from_stratum_manager(&manager);
    assert(influxdb->m_stats.best_difficulty == static_cast<float>(session));
    assert(influxdb->m_stats.total_best_difficulty == static_cast<float>(persisted));
    assert(influxdb->m_stats.accepted == 11);
    assert(influxdb->m_stats.not_accepted == 2);
    assert(influxdb->m_stats.pool_errors == 3);
    assert(influxdb->m_stats.difficulty == 4096);
    assert(influxdb->m_stats.duplicate_hashes == 4);
    assert(influxdb->m_stats.total_uptime == 123);
    assert(influxdb->m_stats.total_blocks_found == 7);
}
int main() {
    Influx db;
    db.m_stats = {};
    db.m_stats.total_uptime = 123;
    db.m_stats.total_blocks_found = 7;
    influxdb = &db;
    StratumManager manager;
    check(manager, 170794, 9158823, 3257420); // reported stale remote value
    check(manager, 0, 9158823, 0); // reboot, empty/new Influx history
    check(manager, 0, 9158823, 100000000.0f); // stale higher remote value is not authoritative
    check(manager, 12345678, 12345678, 9158823); // new record
    check(manager, 0, 12345678, 12345678); // session reset preserves all-time
    check(manager, 0, 0, 12345678); // explicit all-time reset must not resurrect history
    check(manager, 0, UINT64_C(9007199254740991), 0); // existing float wire precision
}
'''
with tempfile.TemporaryDirectory(prefix='influx-best-', dir=os.environ.get('TMPDIR')) as tmp:
    cpp = Path(tmp) / 'test.cpp'
    cpp.write_text(source)
    exe = Path(tmp) / 'test'
    subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-I', str(ROOT / 'components/influx/include'), str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('PASS: production telemetry fetch, 7 persistence/reset/precision cases and unrelated counters')
