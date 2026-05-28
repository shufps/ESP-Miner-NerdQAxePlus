export interface IInfluxDB {
    enabled: number;
    url: string;
    port: number;
    token: string;
    bucket: string;
    org: string;
    prefix: string;
}
