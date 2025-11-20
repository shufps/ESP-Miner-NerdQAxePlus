export interface IPool {
    connected: boolean,
    poolDiffErr: boolean,
    poolDifficulty: number,
    accepted: number,
    rejected: number,
    bestDiff: number,
    pingRtt: number,
    pingLoss: number,
};

export interface IStratum {
    poolMode: number,
    poolBalance?: number, // dual-pool only
    usingFallback?: boolean, // prim/fb only
    totalBestDiff: number,
    pools: IPool[],
}