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
    title: 'System',
    icon: 'menu-outline',
    link: '/pages/system',
  },





/*
  {
    title: 'Auth',
    icon: 'lock-outline',
    link: '/auth',
  },
  {
    title: 'Not found',
    icon: 'close-circle-outline',
    link: '/pages/whatever',
  }*/
];
