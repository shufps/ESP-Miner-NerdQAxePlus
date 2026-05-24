export interface IPool {
    connected: boolean,
    verifyBlocked?: string, // "" | "address_not_found" | "fee_exceeded"
    poolDiffErr: boolean,
    poolDifficulty: number,
    accepted: number,
    rejected: number,
    bestDiff: number,
    pingRtt: number,
    pingLoss: number,
    activeProtocol: number, // running protocol (0=SV1, 1=SV2) — may differ from config until reboot
    // for compatibility reasons only transient here
    // to not have duplicated data in the info endpoint
    host?: string,
    port?: number,
    user?: string,
};

export interface IStratum {
    poolMode: number,
    activePoolMode: number,
    poolBalance?: number, // dual-pool only
    usingFallback?: boolean, // prim/fb only
    totalBestDiff: number,
    pools: IPool[],
}
