export interface ISystemV2Network {
    hostname: string;
    ssid: string;
    macAddr: string;
    ipAddr: string;
    wifiStatus: string;
    wifiRSSI: number;
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
    memory: ISystemV2Memory;
}
