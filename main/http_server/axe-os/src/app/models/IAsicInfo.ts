export interface AsicInfo {
  ASICModel: string;
  deviceModel: string;
  asicCount: number;
  swarmColor : string
  defaultFrequency: number;
  defaultVoltage: number;
  absMaxFrequency: number;
  absMaxVoltage: number;
  ecoFrequency?: number;
  ecoVoltage?: number;
  frequencyOptions: number[];
  voltageOptions: number[];
}
