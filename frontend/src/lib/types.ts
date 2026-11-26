export interface User {
  id: number;
  username: string;
  email?: string;
  fullName: string;
  bio: string;
  website: string;
  profilePicture: string;
  isPrivate: boolean;
  isVerified: boolean;
  createdAt?: string;
  postsCount?: number;
  followersCount?: number;
  followingCount?: number;
  isFollowing?: boolean;
  isOwnProfile?: boolean;
  followStatus?: 'none' | 'pending' | 'accepted' | 'rejected';
  mutualCount?: number;
}

export interface FollowStatus {
  targetId: number;
  isFollowing: boolean;
  followStatus: 'none' | 'pending' | 'accepted' | 'rejected';
  followersCount: number;
  followingCount: number;
  mutualCount: number;
  isOwnProfile: boolean;
}

export interface Media {
  id: number;
  url: string;
  type: 'image' | 'video';
  position: number;
}

export interface Post {
  id: number;
  userId: number;
  username: string;
  avatar: string;
  caption: string;
  location: string;
  createdAt: string;
  media: Media[];
  hashtags: string[];
  likeCount: number;
  commentCount: number;
  likedByViewer: boolean;
  savedByViewer: boolean;
  score?: number;
}

export interface Comment {
  id: number;
  postId?: number;
  userId: number;
  username: string;
  avatar: string;
  body: string;
  createdAt: string;
}

export interface Story {
  id: number;
  userId: number;
  username: string;
  avatar: string;
  mediaUrl: string;
  mediaType: 'image' | 'video';
  createdAt: string;
  expiresAt: string;
  viewCount: number;
  viewedByViewer: boolean;
}

export interface Reel {
  id: number;
  userId: number;
  username: string;
  avatar: string;
  videoUrl: string;
  thumbnailUrl: string;
  caption: string;
  views: number;
  likes: number;
  comments: number;
  likedByViewer: boolean;
  savedByViewer: boolean;
  createdAt: string;
  score?: number;
}

export interface Conversation {
  id: number;
  otherId: number;
  otherUsername: string;
  otherAvatar: string;
  online: boolean;
  lastMessage: string;
  lastMessageAt: string;
  unreadCount: number;
}

export interface Message {
  id: number;
  conversationId: number;
  senderId: number;
  receiverId: number;
  body: string;
  mediaUrl: string;
  isRead: boolean;
  createdAt: string;
}

export interface NotificationItem {
  id: number;
  actorId: number;
  actorUsername: string;
  actorAvatar: string;
  type: string;
  entityId: number;
  body: string;
  isRead: boolean;
  createdAt: string;
}

export interface Hashtag {
  tag: string;
  count: number;
}

export interface SearchResults {
  users: User[];
  hashtags: Hashtag[];
  posts: Post[];
}

export interface ApiErrorBody {
  error?: string;
}
