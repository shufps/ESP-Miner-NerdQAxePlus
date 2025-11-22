export interface IPool {
    connected: boolean,
    poolDiffErr: boolean,
    poolDifficulty: number,
    accepted: number,
    rejected: number,
    bestDiff: number,
    pingRtt: number,
    pingLoss: number,
    // for compatibility reasons only transient here
    // to not have duplicated data in the info endpoint
    host?: string,
    port?: number,
    user?: string,
};

export interface IStratum {
    poolMode: number,
    poolBalance?: number, // dual-pool only
    usingFallback?: boolean, // prim/fb only
    totalBestDiff: number,
    pools: IPool[],
}
