export interface ISystemV2Network {
    hostname: string;
    ssid: string;
    macAddr: string;
    ipAddr: string;
    wifiStatus: string;
    wifiRSSI: number;
    /** device uses ethernet (ipAddr is the ethernet IP then) */
    ethernet: boolean;
}

export interface ISystemV2Stratum {
    /** hosts of the pools currently mining (failover: one, dual pool: both) */
    activePools: string[];
}

export interface ISystemV2Memory {
    freeHeap: number;
    freeHeapInt: number;
}

export interface ISystemV2 {
    deviceModel: string;
    asicModel: string;
    version: string;
    uptimeSeconds: number;
    lastResetReason: string;
    network: ISystemV2Network;
    stratum: ISystemV2Stratum;
    memory: ISystemV2Memory;
}
