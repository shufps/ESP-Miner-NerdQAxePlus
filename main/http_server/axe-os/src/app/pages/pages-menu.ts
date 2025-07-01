import { NbMenuItem } from '@nebular/theme';

export const MENU_ITEMS: NbMenuItem[] = [
  {
    title: 'Dashboard',
    icon: 'home-outline',
    link: '/pages/home',
    home: true,
  },
  {
    title: 'Swarm',
    icon: 'share-outline',
    link: '/pages/swarm',
  },
  {
    title: 'Settings',
    icon: 'settings-2-outline',
    link: '/pages/settings',
  },
  {
    title: 'InfluxDB',
    icon: 'archive',
    link: '/pages/influxdb',
  },
  {
    title: 'Alerts',
    icon: 'bell-outline',
    link: '/pages/alert',
  },
  {
    title: 'System',
    icon: 'menu-outline',
    link: '/pages/system',
  },
];
