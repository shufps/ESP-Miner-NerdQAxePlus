import { eASICModel } from './enum/eASICModel';

export interface ISettingsV2FanPid {
    targetTemp: number;
    p: number;
    i: number;
    d: number;
}

export interface ISettingsV2Fan {
    label?: string;
    mode: number;
    manualSpeed: number;
    overheatTemp: number;
    pid: ISettingsV2FanPid;
}

export interface ISettingsV2Pool {
    url: string;
    port: number;
    user: string;
    enonceSubscribe: number;
    tls: number;
    protocol: number;
    sv2AuthorityPubkey: string;
    sv2ChannelType: number;
    coinbaseVerifyMode: number;
    coinbaseMaxFee: number;
    coinbaseVerifyForce: boolean;
}

export interface ISettingsV2 {
    // Device identity
    asicModel: eASICModel;
    deviceModel: string;
    version: string;
    otp: boolean;
    apActive: boolean;

    // CAN
    can: { hasExtension: boolean; enabled: boolean; };

    // ASIC settings (current + defaults + options — merged from /asic)
    frequency: number;
    coreVoltage: number;
    vrFrequency: number;
    defaultFrequency: number;
    defaultCoreVoltage: number;
    defaultVrFrequency: number;
    ecoFrequency?: number;
    ecoCoreVoltage?: number;
    frequencyOptions: number[];
    voltageOptions: number[];

    // Stratum / pools
    poolMode: number;
    poolBalance: number;
    stratumKeep: number;
    jobInterval: number;
    stratumDifficulty: number;
    pools: ISettingsV2Pool[];

    // Fan / thermal
    fans: ISettingsV2Fan[];
    invertFanPolarity: number;
    pidUseMax: boolean;

    // Network
    hostname: string;
    ssid: string;

    // Mempool
    mempoolCustom: boolean;
    mempoolUrl: string;

    // Display
    flipScreen: number;
    invertScreen: number;
    autoScreenOff: number;
}
