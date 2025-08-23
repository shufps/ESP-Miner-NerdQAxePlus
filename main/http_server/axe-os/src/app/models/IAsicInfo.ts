export interface AsicInfo {
  ASICModel: string;
  deviceModel: string;
  asicCount: number;
  swarmColor : string
  defaultFrequency: number;
  defaultVoltage: number;
  frequencyOptions: number[];
  voltageOptions: number[];
}